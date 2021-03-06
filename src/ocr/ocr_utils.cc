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
#include "util/math.h"
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
                                 [](const auto& line) { return line.baseline.angle; });

    auto is_good_baseline_angle = [=](double angle) {
        return std::abs(angle - mean_baseline_angle) < deg_to_rad(2);
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
        if (!is_good_baseline_angle(line.baseline.angle)) {
            rejected_lines.emplace_back(line.box.y2 + line.baseline.y, i);
            continue;
        }

        auto baseline_at_mid_x = line.box.y2 + line.baseline.y +
                std::tan(line.baseline.angle) * (mid_lines_x - line.box.x1 - line.baseline.x);

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

std::vector<std::pair<double, double>>
    get_all_text_angles(const std::vector<OcrParagraph>& paragraphs)
{
    std::vector<std::pair<double, double>> result;
    for (const auto& par : paragraphs) {
        for (const auto& line : par.lines) {
            for (const auto& word : line.words) {
                result.push_back({word.baseline.angle, word.char_boxes.size()});
            }
        }
    }
    return result;
}

std::pair<double, double> get_dominant_angle(const std::vector<std::pair<double, double>>& angles,
                                             double wrap_around_angle, double window_width)
{
    if (angles.empty()) {
        return {0, 0};
    }

    auto sorted_angles = angles;

    double total_weight = 0;
    for (auto& [angle, weight] : sorted_angles) {
        angle = positive_fmod(angle, wrap_around_angle);
        total_weight += weight;
    }

    std::sort(sorted_angles.begin(), sorted_angles.end());

    double curr_density = 0;
    std::size_t i_begin = 0;
    std::size_t i_end = 0; // exclusive

    // Setup initial window
    while (i_end < sorted_angles.size() && sorted_angles[i_end].first < window_width) {
        curr_density += sorted_angles[i_end].second;
        ++i_end;
    }

    double max_density = curr_density;
    auto max_density_i_begin = i_begin;
    auto max_density_i_end = i_end;

    // Go through all angles looking for the best window
    while (i_end < sorted_angles.size()) {
        auto to_add = sorted_angles[i_end++];
        curr_density += to_add.second;

        while (sorted_angles[i_begin].first <= to_add.first - window_width) {
            curr_density -= sorted_angles[i_begin].second;
            i_begin++;
        }

        if (curr_density > max_density) {
            max_density = curr_density;
            max_density_i_begin = i_begin;
            max_density_i_end = i_end;
        }
    }

    // At this point of time we've investigated all possible windows in the range of
    // [0 .. 2*pi] (assuming sorted_angles contains proper values). We need to also wrap the
    // range and also investigate the range [2*pi, 2*pi + window_width].
    i_end = 0;
    while (i_begin < sorted_angles.size() && i_end < sorted_angles.size()) {
        auto to_add = sorted_angles[i_end++];
        curr_density += to_add.second;

        while (i_begin < sorted_angles.size() &&
               sorted_angles[i_begin].first <= to_add.first + wrap_around_angle - window_width) {
            curr_density -= sorted_angles[i_begin].second;
            i_begin++;
        }

        if (curr_density > max_density) {
            max_density = curr_density;
            max_density_i_begin = i_begin;
            max_density_i_end = i_end;
        }
    }

    double value_sum = 0;
    double weight_sum = 0;
    if (max_density_i_begin < max_density_i_end) {
        for (auto i = max_density_i_begin; i < max_density_i_end; ++i) {
            value_sum += sorted_angles[i].first * sorted_angles[i].second;
            weight_sum += sorted_angles[i].second;
        }
    } else {
        for (auto i = max_density_i_begin; i < sorted_angles.size(); ++i) {
            value_sum += (sorted_angles[i].first - wrap_around_angle) * sorted_angles[i].second;
            weight_sum += sorted_angles[i].second;
        }
        for (auto i = 0; i < max_density_i_end; ++i) {
            value_sum += sorted_angles[i].first * sorted_angles[i].second;
            weight_sum += sorted_angles[i].second;
        }
    }
    double avg = value_sum / weight_sum;
    return {avg, weight_sum / total_weight};
}

double get_average_text_angle(const std::vector<OcrParagraph>& paragraphs)
{
    double angle_accum = 0;
    std::uint64_t total_char_count = 0;
    for (const auto& par : paragraphs) {
        for (const auto& line : par.lines) {
            std::uint64_t line_char_count = 0;
            for (const auto& word : line.words) {
                line_char_count += word.char_boxes.size();
            }
            angle_accum += line_char_count * line.baseline.angle;
            total_char_count += line_char_count;
        }
    }
    return angle_accum / total_char_count;
}

double text_rotation_adjustment(const cv::Mat& image,
                                const std::vector<OcrParagraph>& recognized,
                                const OcrOptions& options)
{
    if (!options.fix_page_orientation && !options.fix_text_rotation) {
        return 0;
    }

    auto all_text_angles = get_all_text_angles(recognized);

    if (options.fix_page_orientation) {
        auto [angle, in_window] = get_dominant_angle(all_text_angles,
                                                     deg_to_rad(360), deg_to_rad(5));
        angle = near_zero_fmod(angle, deg_to_rad(360));
        double angle_mod90 = near_zero_fmod(angle, deg_to_rad(90));
        if (std::abs(angle_mod90) < options.fix_page_orientation_max_angle_diff &&
            in_window > options.fix_page_orientation_min_text_fraction) {

            double adjust_angle = angle - angle_mod90;

            if (std::abs(angle_mod90) < options.fix_text_rotation_max_angle_diff &&
                in_window > options.fix_text_rotation_min_text_fraction)
            {
                adjust_angle += angle;
            }
            return adjust_angle;
        }
    }

    if (options.fix_text_rotation) {
        auto [angle, in_window] = get_dominant_angle(all_text_angles,
                                                     deg_to_rad(90), deg_to_rad(5));
        angle = near_zero_fmod(angle, deg_to_rad(360));
        if (std::abs(angle) < options.fix_text_rotation_max_angle_diff &&
            in_window > options.fix_text_rotation_min_text_fraction)
        {
            return angle;
        }
    }
    return 0;
}

} // namespace sanescan
