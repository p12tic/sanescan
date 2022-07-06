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

cv::Mat image_rotate_centered_noflip(const cv::Mat& image, double angle_rad)
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

cv::Mat image_rotate_centered(const cv::Mat& image, double angle_rad)
{
    if (angle_rad == 0) {
        return image;
    }

    angle_rad = near_zero_fmod(angle_rad, deg_to_rad(360));
    double angle_mod90 = near_zero_fmod(angle_rad, deg_to_rad(90));

    // Use approximate comparison so that computation accuracy does not affect the
    // comparison results.
    double eps = 0.1;


    // First we want to rotate whole page which changes the dimensions of the image. We use
    // cv::rotate to rotate 90, 180 or 270 degrees and then rotate_image for the final adjustment.

    if (angle_rad - angle_mod90 > deg_to_rad(270 - eps)) {
        angle_rad -= deg_to_rad(270);
        cv::rotate(image, image, cv::ROTATE_90_CLOCKWISE);
    } else if (angle_rad - angle_mod90 > deg_to_rad(180 - eps)) {
        angle_rad -= deg_to_rad(180);
        cv::rotate(image, image, cv::ROTATE_180);
    } else if (angle_rad - angle_mod90 > deg_to_rad(90 - eps)) {
        angle_rad -= deg_to_rad(90);
        cv::rotate(image, image, cv::ROTATE_90_COUNTERCLOCKWISE);
    }

    return image_rotate_centered_noflip(image, angle_rad);
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
