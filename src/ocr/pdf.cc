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

    ----------------------------------------------------------------------
    The implementation has been partially based on pdfrenderer.cpp from the
    Tesseract OCR project <https://github.com/tesseract-ocr/tesseract>

    (C) Copyright 2011, Google Inc.
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "pdf.h"
#include "pdf_writer.h"
#include "pdf_ttf_font.h"
#include <podofo/podofo.h>
#include <iosfwd>

namespace sanescan {

void write_pdf(std::ostream& stream, const cv::Mat& image,
               const std::vector<OcrParagraph>& recognized, WritePdfFlags flags)
{
    PdfWriter writer(stream);
    writer.write_header();
    writer.write_page(image, recognized, flags);
}

} // namespace sanescan
