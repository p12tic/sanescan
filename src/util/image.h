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

#ifndef SANESCAN_UTIL_IMAGE_H
#define SANESCAN_UTIL_IMAGE_H

#include <opencv2/core/mat.hpp>

namespace sanescan {

cv::Mat image_rotate_centered_noflip(const cv::Mat& image, double angle_rad);

/** Rotates image preferring flips that potentially change image dimensions for part of the rotation
    thati is a multiple of 90 degrees
*/
cv::Mat image_rotate_centered(const cv::Mat& image, double angle_rad);

/// Converts image to gray, if needed
cv::Mat image_color_to_gray(const cv::Mat& image);

} // namespace sanescan

#endif // SANESCAN_UTIL_MATH_H
