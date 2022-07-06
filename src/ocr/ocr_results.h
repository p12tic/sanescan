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

#ifndef SANESCAN_OCR_OCR_RESULTS_H
#define SANESCAN_OCR_OCR_RESULTS_H

#include "blur_detection.h"
#include "ocr_paragraph.h"
#include <opencv2/core/mat.hpp>
#include <vector>

namespace sanescan {

struct OcrResults {
    /** The image that was used for the OCR. It may differ from the input image as the OCR
        algorithm automatically recognizes rotation to make text horizontal and other cases.
    */
    cv::Mat adjusted_image;

    // The counter-clockwise rotation angle to get the adjusted_image from the source image.
    double adjust_angle = 0;

    // Recognized paragraphs
    std::vector<OcrParagraph> paragraphs;

    // Paragraphs without false positives which have been excluded
    std::vector<OcrParagraph> adjusted_paragraphs;

    // Internal data for blur detection computed from adjusted_image.
    BlurDetectData blur_data;

    // Words that are blurred.
    std::vector<OcrBox> blurred_words;
};

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_RESULTS_H
