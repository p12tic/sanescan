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

#ifndef SANESCAN_OCR_BLUR_DETECTION_H
#define SANESCAN_OCR_BLUR_DETECTION_H

#include "ocr_paragraph.h"
#include <opencv2/core/mat.hpp>

namespace sanescan {

struct BlurDetectData {
    cv::Mat image;
    cv::Mat sobel_transform;
};

BlurDetectData compute_blur_data(cv::Mat image);

/** Detects areas that are under excessive blur for OCR to be effective.

    The detection algorithm utilizes the fact that the appearance of text is bimodal - foreground
    letters on a background, with other colors only in the areas of transition between the two.
    In each character is at least one transition from background to foreground and back. The
    wider this transition, the more blurry the character appears. In the transition area,
    the average value of the first derivative of image data can be approximated as
    {avg_deriv} = {foreground_intensity} - {background_intensity} / {transition_width}.

    We model {transition_width} as {char_width} * {blur_detection_coef} where {blur_detection_coef}
    is arbitrary coefficient. Blurry areas are those where the computed first derivative of the
    data is less than expected {avg_deriv}.
 */
std::vector<OcrBox> detect_blur_areas(const BlurDetectData& data,
                                      const std::vector<OcrParagraph>& recognized,
                                      double blur_detection_coef);

} // namespace sanescan

#endif // SANESCAN_OCR_BLUR_DETECTION_H
