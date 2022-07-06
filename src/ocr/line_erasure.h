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

#ifndef SANESCAN_OCR_LINE_ERASURE_H
#define SANESCAN_OCR_LINE_ERASURE_H

#include <opencv2/core/mat.hpp>

namespace sanescan {

void erase_straight_vh_lines(cv::Mat& image, const cv::Mat& image_gray,
                             int removed_artifact_radius, int extra_width, int line_length);

} // namespace sanescan

#endif // SANESCAN_OCR_LINE_ERASURE_H
