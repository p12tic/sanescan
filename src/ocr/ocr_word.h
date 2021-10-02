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

#ifndef SANESCAN_OCR_OCR_WORD_H
#define SANESCAN_OCR_OCR_WORD_H

#include "ocr_box.h"
#include <string>
#include <vector>

namespace sanescan {

struct OcrWord {
    std::vector<OcrBox> char_boxes;
    OcrBox box;

    // Confidence in the interval [0..1]
    double confidence = 1;

    // Baseline is calculated by taking bottom left corner as origin (X0, Y0) and drawing a line
    // Y = Y0 + baseline_y + (X - X0) * baseline_coeff
    double baseline_y = 0;
    double baseline_coeff = 1;

    // UTF-8 encoded content of the word. If the number of characters equals to char_boxes.size()
    // then each character is placed into one box. Otherwise all characters are placed to the word
    // box.
    std::string content;

    auto operator<=>(const OcrWord&) const = default;
};

std::ostream& operator<<(std::ostream& stream, const OcrWord& word);

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_WORD_H
