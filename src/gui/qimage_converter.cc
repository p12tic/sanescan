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

#include "qimage_converter.h"
#include <QtGui/QImage>
#include <opencv2/core/mat.hpp>
#include <cstring>
#include <stdexcept>

namespace sanescan {

namespace {

using ConversionFunction = std::function<void(char*, const char*, std::size_t)>;

struct ConversionParams {
    int format;
    ConversionFunction converter;
};

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

} // namespace

ConversionParams get_conversion(const SaneParameters& params)
{
    switch (params.frame) {
        case SaneFrameType::GRAY:
            switch (params.depth) {
                case 1:
                    return {CV_8UC1, QImageConverter::convert_mono1};
                case 8:
                    return {CV_8UC1, QImageConverter::convert_mono8};
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
                    return {CV_8UC3, QImageConverter::convert_rgb888};
                case 16:
                    return {CV_16UC4, QImageConverter::convert_rgb161616};
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

struct QImageConverter::Impl {
    cv::Mat image;
    QImage qimage;
    ConversionFunction line_converter;
    SaneParameters params;
};

QImageConverter::QImageConverter() :
    impl_{std::make_unique<Impl>()}
{}

QImageConverter::~QImageConverter() = default;

void QImageConverter::start_frame(const SaneParameters& params, cv::Scalar init_color)
{
    impl_->params = params;
    auto lines = impl_->params.lines > 0 ? impl_->params.lines : 16;
    auto conversion_params = get_conversion(params);
    impl_->line_converter = conversion_params.converter;

    impl_->image = cv::Mat(lines, impl_->params.pixels_per_line, conversion_params.format,
                           init_color);
    impl_->qimage = qimage_from_cv_mat(impl_->image);
}

void QImageConverter::add_line(std::size_t line_index, const char* data, std::size_t data_size)
{
    if (line_index >= impl_->image.size.p[0]) {
        grow_image(line_index);
    }

    impl_->line_converter(reinterpret_cast<char*>(impl_->image.ptr(line_index)),
                          data, std::min<std::size_t>(data_size, impl_->params.bytes_per_line));
}

const QImage& QImageConverter::image() const
{
    return impl_->qimage;
}

void QImageConverter::grow_image(std::size_t min_height)
{
    auto new_height = std::max<std::size_t>(min_height, impl_->image.size.p[0] * 1.5);
    impl_->image.resize(new_height);
    impl_->qimage = qimage_from_cv_mat(impl_->image);
}


void QImageConverter::convert_mono1(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void QImageConverter::convert_mono8(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void QImageConverter::convert_rgb888(char* dst, const char* src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

void QImageConverter::convert_rgb161616(char* dst, const char* src, std::size_t bytes)
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
