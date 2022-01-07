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

#include "file_loader_pdf.h"
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <stdexcept>

namespace sanescan {

struct FileLoaderPdf::Private {
    std::string path;
    std::unique_ptr<poppler::document> document;
};


FileLoaderPdf::FileLoaderPdf(const std::string& path) :
    data_{std::make_unique<Private>()}
{
    data_->path = path;
}

FileLoaderPdf::~FileLoaderPdf() = default;

cv::Mat FileLoaderPdf::load()
{
    if (!data_->document) {
        data_->document.reset(poppler::document::load_from_file(data_->path));
        if (!data_->document) {
            throw std::runtime_error("Could not load PDF file");
        }
    }

    auto* page = data_->document->create_page(0);

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing);

    // TODO: check for a way to not copy memory.
    // TODO: need to send DPI to this API
    poppler::image page_image = renderer.render_page(page, 96, 96);
    cv::Mat image;
    if (page_image.format() == poppler::image::format_rgb24) {
        cv::Mat(page_image.height(), page_image.width(), CV_8UC3, page_image.data()).copyTo(image);
    } else if(page_image.format() == poppler::image::format_argb32) {
        cv::Mat(page_image.height(), page_image.width(), CV_8UC4, page_image.data()).copyTo(image);
    } else {
        throw std::runtime_error("PDF file uses unsupported image format");
    }
    return image;
}

} // namespace sanescan
