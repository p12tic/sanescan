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

#ifndef SANESCAN_OCR_TESSERACT_H
#define SANESCAN_OCR_TESSERACT_H

#include "ocr_paragraph.h"
#include "ocr_options.h"
#include <opencv2/core/mat.hpp>
#include <memory>
#include <vector>

namespace sanescan {

class TesseractRecognizer {
public:
    TesseractRecognizer(const std::string& tesseract_datapath);
    ~TesseractRecognizer();

    // Recognizes text in the given image. Returns the image that should be used for display.
    // It may not be the same image as input image in cases when the rotation is adjusted to make
    // text level and in similar cases.
    std::pair<cv::Mat, std::vector<OcrParagraph>> recognize(cv::Mat image,
                                                            const OcrOptions& options);

private:
    void adjust_image_rotation(cv::Mat& image, std::vector<OcrParagraph>& recognized,
                               OcrOptions options);

    std::vector<OcrParagraph> recognize_internal(const cv::Mat& image);

    struct Private;
    std::unique_ptr<Private> data_;
};


} // namespace sanescan

#endif // SANESCAN_OCR_TESSERACT_H
