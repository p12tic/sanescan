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

#include "qimage_utils.h"
#include <stdexcept>
#include <string>

namespace sanescan {

namespace {

QImage::Format qimage_format_from_depth_channels(int depth, int channels)
{
    if (depth == 1 && channels == 1) {
        return QImage::Format_Grayscale8;
    }
    if (depth == 1 && channels == 3) {
        return QImage::Format_RGB888;
    }
    if (depth == 2 && channels == 4) {
        return QImage::Format_RGBX64;
    }
    throw std::invalid_argument("Unsupported depth+channels combination " + std::to_string(depth) +
                                " " + std::to_string(channels));
}

} // namespace

QImage qimage_from_cv_mat(const cv::Mat& mat)
{
    if (mat.empty()) {
        return {};
    }

    if (mat.size.dims() != 2) {
        throw std::invalid_argument("Unsupported number of dimensions");
    }

    return QImage(mat.data, mat.size.p[1], mat.size.p[0],
                  qimage_format_from_depth_channels(mat.elemSize1(), mat.channels()));
}

QRectF qrectf_from_cv_rect2d(const cv::Rect2d& rect)
{
    return QRectF{rect.x, rect.y, rect.width, rect.height};
}

} // namespace sanescan
