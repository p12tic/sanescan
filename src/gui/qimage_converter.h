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

#ifndef SANESCAN_GUI_IMAGE_CONVERTER_H
#define SANESCAN_GUI_IMAGE_CONVERTER_H

#include "../lib/sane_types.h"
#include <opencv2/core/types.hpp>
#include <memory>

class QImage;

namespace sanescan {

class QImageConverter {
public:
    QImageConverter();
    ~QImageConverter();

    void start_frame(const SaneParameters& params, cv::Scalar init_color);
    void add_line(std::size_t line_index, const char* data, std::size_t data_size);

    const QImage& image() const;

    static void convert_mono1(char* dst, const char* src, std::size_t bytes);
    static void convert_mono8(char* dst, const char* src, std::size_t bytes);
    static void convert_rgb888(char* dst, const char* src, std::size_t bytes);
    static void convert_rgb161616(char* dst, const char* src, std::size_t bytes);
private:

    void grow_image(std::size_t min_height);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_IMAGE_CONVERTER_H
