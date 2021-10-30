/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Povilas Kanapickas <povilas@radix.lt>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ocr_utils.h"
#include <boost/math/constants/constants.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sanescan {

namespace {

template<class T, class Iterator, class F>
T compute_mean(Iterator begin, Iterator end, F&& f)
{
    T sum = static_cast<T>(0);
    std::size_t count = 0;
    while (begin != end) {
        sum += f(*begin++);
        count++;
    }
    if (count == 0) {
        return static_cast<T>(0);
    }
    return sum / count;
}

} // namespace

OcrParagraph sort_paragraph_text(const OcrParagraph& source)
{
    if (source.lines.empty()) {
        return source;
    }

    auto mean_baseline_angle =
            compute_mean<double>(source.lines.begin(), source.lines.end(),
                                 [](const auto& line) { return line.baseline_angle; });

    auto is_good_baseline_angle = [=](double angle) {
        auto diff_limit = 2.0 * boost::math::constants::pi<double>() / 180.0;
        return std::abs(angle - mean_baseline_angle) < diff_limit;
    };

    auto mean_font_size = compute_mean<double>(source.lines.begin(), source.lines.end(),
                                               [](const OcrLine& line) {
        return compute_mean<double>(line.words.begin(), line.words.end(),
                                    [](const OcrWord& word) {
            return word.font_size;
        });
    });

    auto min_lines_x = std::min_element(source.lines.begin(), source.lines.end(),
                                        [](const auto& l, const auto& r) {
        return l.box.x1 < r.box.x1;
    })->box.x1;

    auto max_lines_x = std::max_element(source.lines.begin(), source.lines.end(),
                                        [](const auto& l, const auto& r) {
        return l.box.x2 < r.box.x2;
    })->box.x2;

    // pick the middle X to reduce the magnitude of errors
    auto mid_lines_x = double(min_lines_x + max_lines_x) / 2;

    // stores baseline at line X1 and line index
    std::vector<std::pair<double, std::size_t>> rejected_lines;

    // stores baseline it mid X and line index
    std::vector<std::pair<double, std::size_t>> baselines_y_at_mid_x;
    for (std::size_t i = 0; i < source.lines.size(); ++i) {
        const auto& line = source.lines[i];
        if (!is_good_baseline_angle(line.baseline_angle)) {
            rejected_lines.emplace_back(line.box.y2 + line.baseline_y, i);
            continue;
        }

        auto baseline_at_mid_x = line.box.y2 + line.baseline_y +
                                 std::tan(line.baseline_angle) * (mid_lines_x - line.box.x1 - line.baseline_x);

        baselines_y_at_mid_x.emplace_back(baseline_at_mid_x, i);
    }

    if (baselines_y_at_mid_x.size() < 3) {
        return source;
    }

    auto min_baselines_y =
            std::min_element(baselines_y_at_mid_x.begin(), baselines_y_at_mid_x.end(),
                             [](const auto& l, const auto& r) { return l.first < r.first; })->first;

    auto max_baselines_y =
            std::max_element(baselines_y_at_mid_x.begin(), baselines_y_at_mid_x.end(),
                             [](const auto& l, const auto& r) { return l.first < r.first; })->first;

    // The algorithm is primitive and we don't want too many buckets to be created
    if (mean_font_size < 10 || (max_baselines_y - min_baselines_y) / mean_font_size > 200) {
        return source;
    }

    // The current clustering algorithm is naive as it clusters the lines using a greedy approach.
    // However it works relatively well in practice. The algorith is as follows:
    //
    // A histogram of line Y baselines is created and then the following is performed until there
    // are no nonzero items in the histogram left:
    //
    // - Slide a window of mean_font_size / 2
    // - Pick the window where the it covered largest number of Y baselines
    // - Pick the location Ya that is the average location of the Y baselines covered by window in
    //   the previous step
    // - Cluster all baselines that fall into [Ya - mean_font_size / 2, Ya + mean_font_size / 2]
    // - Remove the clustered baselines from the histogram.
    // - Repeat
    auto buckets_per_line = 10;
    auto histogram_bucket_size = (max_baselines_y - min_baselines_y) / (mean_font_size * buckets_per_line);
    auto histogram_bucket_count = (max_baselines_y - min_baselines_y) / histogram_bucket_size + 2;

    std::vector<std::uint32_t> histogram;
    histogram.resize(histogram_bucket_count);
    std::size_t remaining_histogram_size = baselines_y_at_mid_x.size();

    for (const auto& baseline : baselines_y_at_mid_x) {
        histogram[(baseline.first - min_baselines_y) / histogram_bucket_size]++;
    }

    std::vector<std::pair<double, std::vector<std::size_t>>> line_clusters;

    while (remaining_histogram_size > 0) {
        auto window_size = buckets_per_line / 2;
        std::size_t best_window_start = 0;
        std::size_t best_window_baseline_count = 0;

        // Find the window with most lines falling into it.
        for (std::size_t window_start = 0; window_start + window_size < histogram.size();
             ++window_start)
        {
            auto window_baseline_count =
                    std::accumulate(histogram.begin() + window_start,
                                    histogram.begin() + window_start + window_size, std::size_t(0));
            if (window_baseline_count > best_window_baseline_count) {
                best_window_baseline_count = window_baseline_count;
                best_window_start = window_start;
            }
        }

        // Usually window end can't be outside the histogram unless the histogram itself is too
        // small
        std::size_t best_window_end = std::min(best_window_start + window_size, histogram.size());

        // Find the center of the window and thus cluster bounds
        double window_pos_accum = 0;
        for (std::size_t i = best_window_start; i < best_window_end; ++i) {
            window_pos_accum += i * histogram[i];
        }

        double window_avg_pos = std::round(window_pos_accum / best_window_baseline_count);
        auto cluster_i_min =
                static_cast<std::size_t>(std::max(0.0, window_avg_pos - buckets_per_line / 2));
        auto cluster_i_max =
                static_cast<std::size_t>(std::min(static_cast<double>(histogram.size()),
                                                  window_avg_pos + 1 + buckets_per_line / 2));

        // Assign lines to clusters. We don't care about repeated iteration over
        // baselines_y_at_mid_x as the number of items there is likely to be small.
        auto cluster_baseline_min = min_baselines_y + cluster_i_min * histogram_bucket_size;
        auto cluster_baseline_max = min_baselines_y + cluster_i_max * histogram_bucket_size;
        auto cluster_baseline_center = (cluster_baseline_min + cluster_baseline_max) / 2;

        std::vector<std::size_t> clustered_lines;

        for (auto& line : baselines_y_at_mid_x) {
            if (std::isnan(line.first)) {
                continue;
            }
            if (line.first >= cluster_baseline_min && line.first <= cluster_baseline_max) {
                line.first = std::numeric_limits<double>::quiet_NaN();
                clustered_lines.push_back(line.second);
            }
        }
        line_clusters.emplace_back(cluster_baseline_center, std::move(clustered_lines));

        for (std::size_t i = cluster_i_min; i < cluster_i_max; ++i) {
            remaining_histogram_size -= histogram[i];
            histogram[i] = 0;
        }
    }

    // Sort lines within clusters by their X coordinate and then combine into single lines
    for (auto& line_cluster : line_clusters) {
        auto& lines = line_cluster.second;
        std::sort(lines.begin(), lines.end(), [&](auto l_index, auto r_index) {
            return source.lines[l_index].box.x1 < source.lines[r_index].box.x1;
        });
    }

    // Add rejected lines and then sort the final set of lines by the baseline Y coordinate.
    for (const auto& rejected_line : rejected_lines) {
        line_clusters.push_back({rejected_line.first, {rejected_line.second}});
    }

    std::sort(line_clusters.begin(), line_clusters.end(),
              [](const auto& l, const auto& r){ return l.first < r.first; });

    OcrParagraph result;
    for (const auto& line_cluster : line_clusters) {
        for (auto line_index : line_cluster.second) {
            result.lines.push_back(source.lines[line_index]);
        }
    }

    return result;
}

} // namespace sanescan
