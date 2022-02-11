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

#include "scan_image_buffer.h"
#include <opencv2/core/mat.hpp>
#include <cstring>
#include <stdexcept>

namespace sanescan {

using ConversionFunction = std::function<void(char*, const char*, std::size_t)>;

namespace {

struct ConversionParams {
    int format;
    ConversionFunction converter;
};

ConversionParams get_conversion(const SaneParameters& params)
{
    switch (params.frame) {
        case SaneFrameType::GRAY:
            switch (params.depth) {
                case 1:
                    return {CV_8UC1, ScanImageBuffer::convert_mono1};
                case 8:
                    return {CV_8UC1, ScanImageBuffer::convert_mono8};
                // FIXME: uncomment when Qt 5.13 is the minimum supported version
                // case 16:
                //     return QImage::Format_Grayscale16;
                default:
                    throw std::runtime_error("Unsupported bit depth " +
                                             std::to_string(params.depth));
            }

        case SaneFrameType::RED:
        case SaneFrameType::GREEN:
        case SaneFrameType::BLUE: {
            throw std::runtime_error("Split frame types are currently not supported");
        }
        case SaneFrameType::RGB: {
            switch (params.depth) {
                case 8:
                    return {CV_8UC3, ScanImageBuffer::convert_rgb888};
                case 16:
                    return {CV_16UC4, ScanImageBuffer::convert_rgb161616};
                default:
                    throw std::runtime_error("Unsupported bit depth " +
                                             std::to_string(params.depth));
            }
        }
        default: {
            throw std::runtime_error("Unsupported frame type " +
                                     std::to_string(static_cast<int>(params.frame)));
        }
    }
}

} // namespace

struct ScanImageBuffer::Private {
    cv::Mat image;
    ConversionFunction line_converter;
    SaneParameters params;
    std::function<void()> on_resize;
};

ScanImageBuffer::ScanImageBuffer() :
    d_{std::make_unique<Private>()}
{}

ScanImageBuffer::~ScanImageBuffer() = default;

void ScanImageBuffer::set_on_resize_callback(const std::function<void()>& on_resize)
{
    d_->on_resize = on_resize;
}

void ScanImageBuffer::start_frame(const SaneParameters& params, cv::Scalar init_color)
{
    d_->params = params;
    auto lines = d_->params.lines > 0 ? d_->params.lines : 16;
    auto conversion_params = get_conversion(params);
    d_->line_converter = conversion_params.converter;

    d_->image = cv::Mat(lines, d_->params.pixels_per_line, conversion_params.format, init_color);
    if (d_->on_resize) {
        d_->on_resize();
    }
}

void ScanImageBuffer::add_line(std::size_t line_index, const char* data, std::size_t data_size)
{
    if (line_index >= d_->image.size.p[0]) {
        grow_image(line_index);
    }

    d_->line_converter(reinterpret_cast<char*>(d_->image.ptr(line_index)),
                       data, std::min<std::size_t>(data_size, d_->params.bytes_per_line));
}

const cv::Mat& ScanImageBuffer::image() const
{
    return d_->image;
}

void ScanImageBuffer::grow_image(std::size_t min_height)
{
    auto new_height = std::max<std::size_t>(min_height, d_->image.size.p[0] * 1.5);
    d_->image.resize(new_height);
    if (d_->on_resize) {
        d_->on_resize();
    }
}

void ScanImageBuffer::convert_mono1(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void ScanImageBuffer::convert_mono8(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void ScanImageBuffer::convert_rgb888(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void ScanImageBuffer::convert_rgb161616(char* dst, const char* src, std::size_t bytes)
{
    auto* dst16 = reinterpret_cast<std::uint16_t*>(dst);
    auto* src16 = reinterpret_cast<const std::uint16_t*>(src);

    auto pixel_count = bytes / 6;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        *dst16++ = *src16++;
        *dst16++ = *src16++;
        *dst16++ = *src16++;
        dst16++;
    }
}

} // namespace sanescan
