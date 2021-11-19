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
#include "ocr_baseline.h"
#include "util/math.h"
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace sanescan {

namespace {

OcrBox get_box_for_level(const std::unique_ptr<tesseract::ResultIterator>& it,
                         tesseract::PageIteratorLevel level)
{
    int left, top, right, bottom;
    it->BoundingBox(level, &left, &top, &right, &bottom);
    return OcrBox{left, top, right, bottom};
}

OcrBaseline get_baseline(const std::unique_ptr<tesseract::ResultIterator>& it, const OcrBox& box)
{
    int x1, y1, x2, y2;
    if (!it->Baseline(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2)) {
        return OcrBaseline{0, 0, 0};
    }

    double x1d = x1 - box.x1;
    double x2d = x2 - box.x1;
    double y1d = y1 - box.y2;
    double y2d = y2 - box.y2;

    if (x1d == x2d) {
        if (y1d > y2d) {
            return OcrBaseline{x1d, y1d, -deg_to_rad(90)};
        } else {
            return OcrBaseline{x1d, y1d, deg_to_rad(90)};
        }
    }

    return OcrBaseline{x1d, y1d, std::atan((y2d - y1d) / (x2d - x1d))};
}

OcrBaseline adjust_baseline_for_other_box(const OcrBaseline& src_baseline, const OcrBox& src_box,
                                          const OcrBox& dst_box)
{
    if (src_baseline.angle > deg_to_rad(45) || src_baseline.angle < -deg_to_rad(45)) {
        // baseline is more vertical than horizontal, adjust y baseline offset within
        // bounding box to zero
        auto y_diff = dst_box.y2 - (src_box.y2 + src_baseline.y);
        auto baseline_x_diff = -y_diff * std::tan(src_baseline.angle - deg_to_rad(90));
        auto x = src_box.x1 + src_baseline.x - dst_box.x1 + baseline_x_diff;
        return OcrBaseline{x, 0, src_baseline.angle};
    }

    // baseline is more horizontal than vertical, adjust x baseline offset within
    // bounding box to zero
    auto x_diff = dst_box.x1 - (src_box.x1 + src_baseline.x);
    auto baseline_y_diff = x_diff * std::tan(src_baseline.angle);
    auto y = src_box.y2 + src_baseline.y - dst_box.y2 + baseline_y_diff;
    return OcrBaseline{0, y, src_baseline.angle};
}


} // namespace

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
