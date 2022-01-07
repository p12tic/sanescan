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

#include "file_loader_image.h"
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace sanescan {

FileLoaderImage::FileLoaderImage(const std::string& path) : path_{path}
{
}

FileLoaderImage::~FileLoaderImage() = default;

cv::Mat FileLoaderImage::load()
{
    auto image = cv::imread(path_);
    if (image.data == nullptr) {
        throw std::runtime_error("Could not load input file");
    }
    return image;
}

} // namespace sanescan

