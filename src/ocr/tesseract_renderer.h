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

#ifndef SANESCAN_OCR_TESSERACT_RENDERER_H
#define SANESCAN_OCR_TESSERACT_RENDERER_H

#include "ocr_paragraph.h"
#include <tesseract/renderer.h>

namespace sanescan {

class TesseractRenderer : public tesseract::TessResultRenderer {
public:
    explicit TesseractRenderer();

    const std::vector<OcrParagraph>& get_paragraphs() const { return paragraphs_; }
protected:
    bool BeginDocumentHandler() override;
    bool AddImageHandler(tesseract::TessBaseAPI *api) override;
    bool EndDocumentHandler() override;

private:
    std::vector<OcrParagraph> paragraphs_;
};

} // namespace sanescan

#endif // SANESCAN_OCR_TESSERACT_RENDERER_H
