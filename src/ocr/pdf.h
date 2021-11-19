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


#ifndef SANESCAN_OCR_PDF_H
#define SANESCAN_OCR_PDF_H

#include "ocr_paragraph.h"
#include "util/enum_flags.h"
#include <opencv2/core/mat.hpp>
#include <iosfwd>

namespace sanescan {

enum class WritePdfFlags {
    NONE = 0,
    DEBUG_CHAR_BOXES = 1 << 0,
};

SANESCAN_DECLARE_OPERATORS_FOR_SCOPED_ENUM_FLAGS(WritePdfFlags)

void write_pdf(std::ostream& stream, const cv::Mat& image,
               const std::vector<OcrParagraph>& recognized,
               WritePdfFlags flags = WritePdfFlags::NONE);

} // namespace sanescan

#endif // SANESCAN_OCR_PDF_H
