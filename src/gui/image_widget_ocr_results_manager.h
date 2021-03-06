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

#ifndef SANESCAN_GUI_IMAGE_WIDGET_OCR_RESULTS_MANAGER_H
#define SANESCAN_GUI_IMAGE_WIDGET_OCR_RESULTS_MANAGER_H

#include "ocr/ocr_paragraph.h"

#include <QtWidgets/QGraphicsScene>
#include <memory>

namespace sanescan {

class ImageWidgetOcrResultsManager {
public:
    explicit ImageWidgetOcrResultsManager(QGraphicsScene* scene);
    ~ImageWidgetOcrResultsManager();

    void clear();
    void setup(const std::vector<OcrParagraph>& results,
               const std::vector<OcrBox>& blurry_areas);
    void set_show_text(bool show);
    void set_show_text_white_background(bool show);
    void set_show_bounding_boxes(bool show);
    void set_show_blur_warning_boxes(bool show);

private:
    void clear_items(std::vector<QGraphicsItem*>& items);

    void setup_word(const OcrWord& word);
    void setup_blur_warning_area(const OcrBox& area);
    void set_tooltip(QGraphicsItem* item, const OcrWord& word);

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_IMAGE_WIDGET_OCR_RESULTS_MANAGER_H
