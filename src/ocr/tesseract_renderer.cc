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

#include "tesseract_renderer.h"
#include "tesseract_renderer_utils.h"
#include <tesseract/baseapi.h>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace sanescan {

TesseractRenderer::TesseractRenderer() : tesseract::TessResultRenderer("-", "")
{
}

bool TesseractRenderer::BeginDocumentHandler()
{
    paragraphs_.clear();
    return true;
}

bool TesseractRenderer::AddImageHandler(tesseract::TessBaseAPI *api)
{
    std::unique_ptr<tesseract::ResultIterator> it(api->GetIterator());

    OcrParagraph* curr_par = nullptr;
    OcrLine* curr_line = nullptr;
    float curr_row_height = 0;

    while (!it->Empty(tesseract::RIL_BLOCK)) {
        int left, top, right, bottom;
        auto block_type = it->BlockType();
        switch (block_type) {
            case tesseract::PT_FLOWING_IMAGE:
            case tesseract::PT_HEADING_IMAGE:
            case tesseract::PT_PULLOUT_IMAGE:
            case tesseract::PT_HORZ_LINE:
            case tesseract::PT_VERT_LINE:
            case tesseract::PT_NOISE: {
                it->Next(tesseract::RIL_BLOCK);
                continue;
            }
            default:
                break;
        }

        if (it->Empty(tesseract::RIL_WORD)) {
            it->Next(tesseract::RIL_WORD);
            continue;
        }

        if (it->IsAtBeginningOf(tesseract::RIL_PARA)) {
            paragraphs_.emplace_back();
            curr_par = &paragraphs_.back();
            curr_par->box = get_box_for_level(it, tesseract::RIL_PARA);
        }

        if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {
            if (curr_par == nullptr) {
                throw std::runtime_error("Unexpectedly unset paragraph");
            }

            curr_par->lines.emplace_back();
            curr_line = &curr_par->lines.back();
            curr_line->box = get_box_for_level(it, tesseract::RIL_TEXTLINE);
            curr_line->baseline = get_baseline(it, curr_line->box);

            float descenders = 0;
            float ascenders = 0;
            it->RowAttributes(&curr_row_height, &descenders, &ascenders);
        }

        if (curr_line == nullptr) {
            throw std::runtime_error("Unexpectedly unset line");
        }

        curr_line->words.emplace_back();
        auto& curr_word = curr_line->words.back();
        curr_word.box = get_box_for_level(it, tesseract::RIL_WORD);
        curr_word.baseline = adjust_baseline_for_other_box(curr_line->baseline, curr_line->box,
                                                           curr_word.box);
        curr_word.font_size = curr_row_height;
        curr_word.confidence = it->Confidence(tesseract::RIL_WORD) / 100.0;

        do {
            std::unique_ptr<const char[]> grapheme(it->GetUTF8Text(tesseract::RIL_SYMBOL));
            if (grapheme && grapheme[0] != 0) {
                curr_word.char_boxes.push_back(get_box_for_level(it, tesseract::RIL_SYMBOL));
                curr_word.content += grapheme.get();
            }
            it->Next(tesseract::RIL_SYMBOL);
        } while (!it->Empty(tesseract::RIL_BLOCK) && !it->IsAtBeginningOf(tesseract::RIL_WORD));
    }
    return true;
}

bool TesseractRenderer::EndDocumentHandler()
{
    return true;
}

} // namespace sanescan
