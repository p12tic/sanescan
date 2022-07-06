/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2022  Povilas Kanapickas <povilas@radix.lt>

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

#include "blur_detection.h"
#include "util/image.h"
#include "util/math.h"
#include <opencv2/imgproc.hpp>

namespace sanescan {

namespace {

std::vector<std::uint64_t> hist_to_vector(const cv::Mat& mat)
{
    std::vector<std::uint64_t> res;
    res.reserve(mat.size.p[0]);
    for (int i = 0; i < mat.size.p[0]; ++i) {
        res.push_back(mat.at<std::size_t>(i, 0));
    }
    return res;
}

std::vector<std::uint64_t> compute_color_hist(const cv::Mat& mat)
{
    cv::Mat hist;
    std::vector<cv::Mat> mat_list = {mat};
    cv::calcHist(mat_list, {0}, cv::Mat(), hist, {255}, {0, 255});
    return hist_to_vector(hist);
}

bool is_word_blurry(const OcrWord& word, const BlurDetectData& data, double blur_detection_coef)
{
    auto word_rect = cv::Rect(word.box.x1, word.box.y1, word.box.width(), word.box.height());

    auto intensity_hist = compute_color_hist(data.image(word_rect));
    auto sobel_hist = compute_color_hist(data.sobel_transform(word_rect));

    auto char_width = std::max(word.box.width(), word.box.height()) / word.char_boxes.size();
    auto min_intensity = index_at_quantile<double>(intensity_hist.begin(), intensity_hist.end(),
                                                   0.05);
    auto max_intensity = index_at_quantile<double>(intensity_hist.begin(), intensity_hist.end(),
                                                   0.95);
    auto curr_intens_diff = max_intensity - min_intensity;

    // remove difference background.
    auto min_sobel_cutoff = curr_intens_diff / char_width;
    for (auto i = 0; i < std::min(min_sobel_cutoff, sobel_hist.size()); i++) {
        sobel_hist[i] = 0;
    }

    auto max_sobel = index_at_quantile<double>(sobel_hist.begin(), sobel_hist.end(), 0.85);

    auto expected_max_blur_width = char_width * blur_detection_coef;

    /* The logical comparison would be:

        auto curr_blur_width = static_cast<double>(max_intensity - min_intensity) / max_sobel;
        return curr_blur_width >= expected_max_blur_width;

        max_sobel may be zero, so we multiply both sides of the comparison by max_sobel to be safe.
    */
    auto expected_max_intens_diff = expected_max_blur_width * max_sobel;
    return curr_intens_diff >= expected_max_intens_diff;
}

} // namespace

BlurDetectData compute_blur_data(cv::Mat image)
{
    BlurDetectData result;
    image = image_color_to_gray(image);
    cv::Mat sobel_x, sobel_y;
    cv::Sobel(image, sobel_x, CV_32F, 1, 0);
    cv::Sobel(image, sobel_y, CV_32F, 0, 1);
    cv::addWeighted(sobel_x, 0.5, sobel_y, 0.5, 0, result.sobel_transform);
    result.image = image;
    return result;
}

std::vector<OcrBox> detect_blur_areas(const BlurDetectData& data,
                                      const std::vector<OcrParagraph>& recognized,
                                      double blur_detection_coef)
{
    std::vector<OcrBox> blurry_boxes;

    for (const auto& par : recognized) {
        for (const auto& line : par.lines) {
            for (const auto& word : line.words) {
                if (is_word_blurry(word, data, blur_detection_coef)) {
                    blurry_boxes.push_back(word.box);
                }
            }
        }
    }
    return blurry_boxes;
}

} // namespace sanescan
