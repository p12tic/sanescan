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

#ifndef SANESCAN_OCR_HOCR_H
#define SANESCAN_OCR_HOCR_H

#include "ocr_paragraph.h"
#include <iosfwd>
#include <stdexcept>

namespace sanescan {

class HocrException : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::vector<OcrParagraph> read_hocr(std::istream& input);

// Produces non-compliant output, only used for checking internal state of the library.
void write_hocr(std::ostream& output, const std::vector<OcrParagraph>& paragraphs);

} // namespace sanescan

#endif
