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

#include "tesseract.h"
#include "ocr_utils.h"
#include "tesseract_renderer.h"
#include "util/image.h"
#include "util/math.h"

#include <leptonica/allheaders.h>
#include <opencv2/imgcodecs.hpp>
#include <tesseract/baseapi.h>
#include <stdexcept>

namespace sanescan {

namespace {

PIX* cv_mat_to_pix(const cv::Mat& image)
{
    if (image.size.dims() != 2) {
        throw std::invalid_argument("Input image must be 2D");
    }
    if (image.elemSize1() != 1) {
        throw std::invalid_argument("Non 8-bit images are not supported");
    }

    auto width = image.size.p[1];
    auto height = image.size.p[0];

    switch (image.channels()) {
        case 1: break;
        case 3: break;
        default: throw std::invalid_argument("Input image must have 1 or 3 channels");
    }

    unsigned channels = 4;
    auto depth = image.elemSize1() * 8 * channels;

    auto* pix = pixCreate(width, height, depth);
    if (pix == nullptr) {
        throw std::runtime_error("Could not create image copy for processing");
    }

    auto* dst_data = pixGetData(pix);
    auto wpl = pixGetWpl(pix);

    if (image.channels() == 1) {
        for (std::size_t row = 0; row < height; ++row) {
            const std::uint8_t* src_ptr = image.ptr(row);
            std::uint32_t* dst_ptr = dst_data + row * wpl;
            std::uint8_t* dst_byte_ptr = reinterpret_cast<std::uint8_t*>(dst_ptr);
            for (std::size_t i = 0; i < width; ++i) {
                auto value = *src_ptr++;
                *dst_byte_ptr++ = value;
                *dst_byte_ptr++ = value;
                *dst_byte_ptr++ = value;
                *dst_byte_ptr++ = 255;
            }
        }
    }

    if (image.channels() == 3) {
        for (std::size_t row = 0; row < height; ++row) {
            const std::uint8_t* src_ptr = image.ptr(row);
            std::uint32_t* dst_ptr = dst_data + row * wpl;
            std::uint8_t* dst_byte_ptr = reinterpret_cast<std::uint8_t*>(dst_ptr);
            for (std::size_t i = 0; i < width; ++i) {
                *dst_byte_ptr++ = *src_ptr++;
                *dst_byte_ptr++ = *src_ptr++;
                *dst_byte_ptr++ = *src_ptr++;
                *dst_byte_ptr++ = 255;
            }
        }
    }

    return pix;
}

} // namespace

struct TesseractRecognizer::Private {
    tesseract::TessBaseAPI tesseract;
};

TesseractRecognizer::TesseractRecognizer(const std::string& tesseract_datapath) :
    data_{std::make_unique<Private>()}
{
    if (data_->tesseract.Init(tesseract_datapath.c_str(), "eng",
                              tesseract::OEM_LSTM_ONLY) != 0) {
        throw std::runtime_error("Tesseract could not initialize");
    }

    data_->tesseract.SetPageSegMode(tesseract::PSM_SPARSE_TEXT_OSD);
}

TesseractRecognizer::~TesseractRecognizer() = default;

OcrResults TesseractRecognizer::recognize(cv::Mat image, const OcrOptions& options)
{
    auto recognized = recognize_internal(image);

    OcrResults results;

    // Handle the case when all text within the image is rotated slightly due to the input data
    // scan just being rotated. In such case whole image will be rotated to address the following
    // issues:
    //
    // - Most PDF readers can't select rotated text properly
    // - The OCR accuracy is compromised for rotated text.
    //
    // TODO: Ideally we should detect cases when the text in the source image is legitimately
    // rotated and the rotation is not just the artifact of rotation. In such case the accuracy of
    // OCR will still be improved if rotate the source image just for OCR and then rotate the
    // results back.
    results.adjust_angle = text_rotation_adjustment(image, recognized, options);

    if (results.adjust_angle != 0) {
        image = image_rotate_centered(image, results.adjust_angle);
        recognized = recognize_internal(image);
    }
    results.adjusted_image = image;
    results.paragraphs = recognized;
    return results;
}

std::vector<OcrParagraph> TesseractRecognizer::recognize_internal(const cv::Mat& image)
{
    auto* pix = cv_mat_to_pix(image);

    TesseractRenderer renderer;
    if (!data_->tesseract.ProcessPage(pix, 0, nullptr, nullptr, 0, &renderer)) {
        throw std::runtime_error("Failed to process page");
    }

    return renderer.get_paragraphs();
}

} // namespace sanescan

