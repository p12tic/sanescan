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

#include "line_erasure.h"
#include <opencv2/imgproc.hpp>

namespace sanescan {

namespace {

void apply_vertical(cv::Mat& image, const cv::Mat& mask)
{
    auto width = image.size.p[1];
    auto height = image.size.p[0];

    std::size_t pixel_size = image.elemSize1() * image.channels();
    std::vector<uchar> fixup_pixels;
    fixup_pixels.resize(pixel_size * width);
    auto* fixup_pixel_ptr = fixup_pixels.data();

    for (std::size_t iy = 1; iy < height; ++iy) {
        const auto* prev_line_ptr = image.ptr(iy - 1);
        auto* curr_line_ptr = image.ptr(iy);

        for (std::size_t ix = 0; ix < width; ++ix) {
            if (mask.at<uchar>(iy, ix) == 0) {
                continue;
            }
            auto* prev_pixel = prev_line_ptr + pixel_size * ix;
            auto* curr_pixel = curr_line_ptr + pixel_size * ix;
            auto* fixup_pixel = fixup_pixel_ptr + pixel_size * ix;

            if (mask.at<uchar>(iy - 1, ix) == 0) {
                std::memcpy(fixup_pixel, prev_pixel, pixel_size);
                std::memcpy(curr_pixel, fixup_pixel, pixel_size);
            } else {
                std::memcpy(curr_pixel, fixup_pixel, pixel_size);
            }
        }
    }
}

void apply_horizontal(cv::Mat& image, const cv::Mat& mask)
{
    auto width = image.size.p[1];
    auto height = image.size.p[0];

    std::size_t pixel_size = image.elemSize1() * image.channels();
    std::uint64_t fixup_pixel = 0;

    for (std::size_t iy = 0; iy < height; ++iy) {
        for (std::size_t ix = 1; ix < width; ++ix) {
            if (mask.at<uchar>(iy, ix) == 0) {
                continue;
            }

            auto* line_ptr = image.ptr(iy);

            auto* prev_pixel = line_ptr + pixel_size * ix;
            auto* curr_pixel = line_ptr + pixel_size * ix;

            if (mask.at<uchar>(iy, ix - 1) == 0) {
                std::memcpy(&fixup_pixel, prev_pixel, pixel_size);
                std::memcpy(curr_pixel, &fixup_pixel, pixel_size);
            } else {
                std::memcpy(curr_pixel, &fixup_pixel, pixel_size);
            }
        }
    }
}

void fixup_dilate_lines(cv::Mat& image, int extra_width)
{
    if (extra_width == 0) {
        return;
    }

    auto kernel_size = extra_width * 2 - 1;
    auto dilate_kernel = cv::getStructuringElement(cv::MORPH_RECT,
                                                   cv::Size{kernel_size, kernel_size});
    cv::morphologyEx(image, image, cv::MORPH_DILATE,
                     dilate_kernel, cv::Point(-1,-1), 1);
}

} // namespace

void erase_straight_vh_lines(cv::Mat& image, const cv::Mat& image_gray,
                             int removed_artifact_radius, int extra_width, int line_length)
{
    cv::Mat thresh_image;
    cv::threshold(image, thresh_image, 0, 255, cv::THRESH_BINARY_INV + cv::THRESH_OTSU);

    if (removed_artifact_radius > 0) {
        int kernel_size = removed_artifact_radius * 2 - 1;
        auto kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size{kernel_size, kernel_size});
        cv::morphologyEx(thresh_image, thresh_image, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), 1);
    }

    auto kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size{line_length, 1});
    cv::Mat detected_lines_v;
    cv::morphologyEx(thresh_image, detected_lines_v, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 2);
    fixup_dilate_lines(detected_lines_v, extra_width);
    apply_vertical(image, detected_lines_v);

    kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size{1, line_length});
    cv::Mat detected_lines_h;
    cv::morphologyEx(thresh_image, detected_lines_h, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 2);
    fixup_dilate_lines(detected_lines_h, extra_width);
    apply_horizontal(image, detected_lines_h);
}

} // namespace sanescan
