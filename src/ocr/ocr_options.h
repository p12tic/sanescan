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

#ifndef SANESCAN_OCR_OCR_OPTIONS_H
#define SANESCAN_OCR_OCR_OPTIONS_H

#include "ocr_word.h"
#include "ocr_baseline.h"
#include "util/math.h"
#include <vector>
#include <iosfwd>

namespace sanescan {

struct OcrOptions {
    /*  True if the source image should be rotated to fix slight text skep (e.g. due to the
        scanned image being placed slightly incorrectly). This is only done if
        both of the following hold:

         - fix_text_rotation is set to true.
         - at least fix_text_rotation_min_text_fraction fraction of all text characters have
           baseline that points to the same direction modulo 90 degrees.
         - the average direction the baselines modulo 90 degree is at most
           fix_text_rotation_max_angle_diff radians from zero degrees.
    */
    bool fix_text_rotation = true;
    double fix_text_rotation_min_text_fraction = 0.95;
    double fix_text_rotation_max_angle_diff = deg_to_rad(5);
    bool keep_image_size_after_rotation = false;

    /*  True if the source image should be rotated to fix page orientation. This is only done if
        both of the following hold:

         - fix_page_orientation is set to true.
         - at least fix_page_orientation_min_text_fraction fraction of all text characters have
           baseline that points to the same direction
         - the average direction the baselines point to is at most
           fix_page_orientation_max_angle_diff radians from 0, 90, 180 or 270 degree angles.
    */
    bool fix_page_orientation = true;
    double fix_page_orientation_min_text_fraction = 0.95;
    double fix_page_orientation_max_angle_diff = deg_to_rad(5);

    //  Minimum confidence of words included into the results
    double min_word_confidence = 0;

    std::strong_ordering operator<=>(const OcrOptions& other) const = default;
};

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_OPTIONS_H
