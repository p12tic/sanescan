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

#include "image.h"
#include "util/math.h"
#include <opencv2/imgproc/imgproc.hpp>

namespace sanescan {

cv::Mat rotate_image_centered(const cv::Mat& image, double angle_rad)
{
    auto height = image.size.p[0];
    auto width = image.size.p[1];

    cv::Mat rotation_mat = cv::getRotationMatrix2D(cv::Point2f(width / 2, height / 2),
                                                   rad_to_deg(angle_rad), 1.0);

    cv::Mat rotated_image;
    cv::warpAffine(image, rotated_image, rotation_mat, image.size(),
                   cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    return rotated_image;
}

cv::Mat image_color_to_gray(const cv::Mat& image)
{
    cv::Mat result;
    if (image.channels() > 1) {
        cv::cvtColor(image, result, cv::COLOR_BGR2GRAY);
    } else {
        result = image;
    }
    return result;
}

} // namespace sanescan
