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

#ifndef SANESCAN_OCR_OCR_UTILS_H
#define SANESCAN_OCR_OCR_UTILS_H

#include "ocr_options.h"
#include "ocr_paragraph.h"

namespace sanescan {

/* Tesseract doesn't handle slightly rotated text well. If lines change their Y position by
   more than line height then lines may be broken into several and then ordered by their
   Y position. This will cause the text being put into the paragraph in wrong order.

   This function will sort the text according to the baseline of the lines.
*/
OcrParagraph sort_paragraph_text(const OcrParagraph& source);

// Returns text angles. The first element of the pair is the angle, the second is arbitrary weight.
std::vector<std::pair<double, double>> get_all_text_angles(const std::vector<OcrParagraph>& paragraphs);

/*  This function calculates the dominant direction of the text.

    It finds the angle for which the range [angle - window_width / 2, angle + window_width / 2]
    contains the largest density of weighted input angles. Then the weighted average of all angles
    within the range are computed and returned as the first element of the returned pair. The
    second element of the returned pair contains the [0, 1] proportion of angles that fall within
    window.

    The input angles are interpreted modulo wrap_araound_angle. This allows to e.g. detect slight
    rotation of a page that contains both horizontal and vertical text.

    If the window with best density is across the angle zero (i.e. the search wrapped around),
    then the average angle may be negative.
*/
std::pair<double, double> get_dominant_angle(const std::vector<std::pair<double, double>>& angles,
                                             double wrap_around_angle, double window_width);

/*  This function retuns the optimal rotation that needs to be applied to image in order for the
    text to become horizontal.
*/
double text_rotation_adjustment(const cv::Mat& image,
                                const std::vector<OcrParagraph>& recognized,
                                const OcrOptions& options);

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_WORD_H
