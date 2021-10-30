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

#ifndef SANESCAN_OCR_OCR_LINE_H
#define SANESCAN_OCR_OCR_LINE_H

#include "ocr_word.h"
#include "ocr_baseline.h"
#include <vector>
#include <iosfwd>

namespace sanescan {

struct OcrLine {
    std::vector<OcrWord> words;
    OcrBox box;
    OcrBaseline baseline;

    auto operator<=>(const OcrLine&) const = default;
};

std::ostream& operator<<(std::ostream& stream, const OcrLine& line);

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_LINE_H
