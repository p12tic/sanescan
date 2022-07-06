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

#include "image_widget_ocr_results_manager.h"
#include "font_metrics_cache.h"
#include "util/math.h"
#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtWidgets/QGraphicsItem>
#include <boost/locale/encoding.hpp>
#include <cmath>

namespace sanescan {

namespace {
    struct ParsedQString {
        std::vector<QString> symbols;
        QString string;
    };

    ParsedQString parse_utf8_string(const std::string& utf8_string)
    {
        // FIXME: ideally we should use ICU to properly split the string into graphemes. Currently
        // we assume that OCR will only output graphemes that correspond to single Unicode code
        // points.
        auto text_utf32 = boost::locale::conv::utf_to_utf<char32_t>(utf8_string);
        ParsedQString parsed;
        parsed.symbols.reserve(text_utf32.size());
        parsed.string.reserve(text_utf32.size() * 2);

        for (std::size_t i = 0; i < text_utf32.size(); ++i) {
            auto ch_utf16 = boost::locale::conv::utf_to_utf<char16_t>(text_utf32.substr(i, 1));
            auto qch_utf16 = QString::fromStdU16String(ch_utf16);

            parsed.string.append(qch_utf16);
            parsed.symbols.push_back(std::move(qch_utf16));
        }
        return parsed;
    }

    struct PositioningParams {
        bool enable_char_positioning = false;
        double h_scale = 1;
    };

    PositioningParams get_character_positioning_params(const FontMetricsCache::Entry& font,
                                                       const ParsedQString& parsed,
                                                       const OcrWord& word)
    {
        auto rect = font.metrics.boundingRect(parsed.string);
        double h_scale = word.box.width() / static_cast<double>(rect.width());

        if (parsed.symbols.size() != word.char_boxes.size()) {
            // If there are different number of recognized symbols compared to character boxes then
            // we can only do word positioning.
            return PositioningParams{false, h_scale};
        }

        if (h_scale < 1.5) {
            // If the text spacing is not too large then it will still appear alright if rendered
            // without character positioning
            return PositioningParams{false, h_scale};
        }

        // Check if any of the character boxes have weird bounds
        for (std::size_t i = 0; i < parsed.symbols.size(); ++i) {
            auto symbol_rect = font.metrics.boundingRect(parsed.symbols[i]);
            const auto& symbol_box = word.char_boxes[i];
            if (symbol_rect.width() > symbol_box.width() * 1.5) {
                return PositioningParams{false, h_scale};
            }
        }

        return PositioningParams{true, 1.0};
    }
} // namespace

struct ImageWidgetOcrResultsManager::Private {
    QGraphicsScene* scene = nullptr;
    FontMetricsCache metrics_cache{"times"};

    QGraphicsItemGroup* text_items_group = nullptr;
    QGraphicsItemGroup* text_background_items_group = nullptr;
    QGraphicsItemGroup* char_bounding_boxes_group = nullptr;
    std::vector<QGraphicsItem*> text_items;
    std::vector<QGraphicsItem*> text_background_items;
    std::vector<QGraphicsItem*> char_bounding_boxes;

    QPen text_background_pen;
    QBrush text_background_brush;
    QPen char_bounding_boxes_pen;
    QBrush char_bounding_boxes_brush;
};

ImageWidgetOcrResultsManager::ImageWidgetOcrResultsManager(QGraphicsScene* scene) :
    d_{std::make_unique<Private>()}
{
    d_->scene = scene;
    d_->text_items_group = d_->scene->createItemGroup({});
    d_->text_items_group->setZValue(2);
    d_->text_background_items_group = d_->scene->createItemGroup({});
    d_->text_background_items_group->setZValue(1);
    d_->char_bounding_boxes_group = d_->scene->createItemGroup({});
    d_->char_bounding_boxes_group->setZValue(3);

    d_->text_background_pen.setStyle(Qt::NoPen);
    d_->text_background_brush.setColor(Qt::white);
    d_->text_background_brush.setStyle(Qt::SolidPattern);
    d_->char_bounding_boxes_pen.setWidth(1);
    d_->char_bounding_boxes_pen.setColor(Qt::black);
    d_->char_bounding_boxes_pen.setStyle(Qt::SolidLine);
    d_->char_bounding_boxes_brush.setStyle(Qt::NoBrush);
}

ImageWidgetOcrResultsManager::~ImageWidgetOcrResultsManager() = default;

void ImageWidgetOcrResultsManager::clear()
{
    clear_items(d_->text_items);
    clear_items(d_->text_background_items);
    clear_items(d_->char_bounding_boxes);
}

void ImageWidgetOcrResultsManager::setup(const std::vector<OcrParagraph>& results)
{
    clear();

    for (const auto& paragraph : results) {
        for (const auto& line : paragraph.lines) {
            for (const auto& word : line.words) {
                setup_word(word);
            }
        }
    }
}

void ImageWidgetOcrResultsManager::set_show_text(bool show)
{
    if (show) {
        d_->text_items_group->show();
    } else {
        d_->text_items_group->hide();
    }
}

void ImageWidgetOcrResultsManager::set_show_text_white_background(bool show)
{
    if (show) {
        d_->text_background_items_group->show();
    } else {
        d_->text_background_items_group->hide();
    }
}

void ImageWidgetOcrResultsManager::set_show_bounding_boxes(bool show)
{
    if (show) {
        d_->char_bounding_boxes_group->show();
    } else {
        d_->char_bounding_boxes_group->hide();
    }
}

void ImageWidgetOcrResultsManager::clear_items(std::vector<QGraphicsItem*>& items)
{
    for (auto item : items) {
        d_->scene->removeItem(item);
        delete item;
    }
    items.clear();
}

void ImageWidgetOcrResultsManager::setup_word(const OcrWord& word)
{
    auto text_utf32 = boost::locale::conv::utf_to_utf<char32_t>(word.content);
    if (text_utf32.empty()) {
        return;
    }

    const auto& font_data = d_->metrics_cache.get_font_for_size(word.font_size);

    // The code below positions character boxes on the canvas. We can't use a
    // simple transform because all coordinates except character baseline are in
    // image coordinates.
    auto angle_sin = std::sin(word.baseline.angle);
    auto angle_cos = std::cos(word.baseline.angle);
    auto angle_tan = angle_sin / angle_cos;

    // Get word coordinates at baseline
    double word_x_baseline = word.box.x1;
    double word_y_baseline = word.box.y2 +
            word.baseline.y - word.baseline.x * angle_tan;

    // Get word coordinates at top left corner
    auto word_x = word_x_baseline + font_data.metrics.ascent() * angle_sin;
    auto word_y = word_y_baseline - font_data.metrics.ascent() * angle_cos;
    auto word_y_for_rect = word_y_baseline - font_data.metrics.capHeight() * angle_cos;

    QRectF text_background_rect{word_x, word_y_for_rect,
                                (word.box.x2 - word.box.x1) / angle_cos, word.font_size};

    auto* word_background_item = d_->scene->addRect(text_background_rect,
                                                    d_->text_background_pen,
                                                    d_->text_background_brush);
    word_background_item->setTransformOriginPoint(word_x, word_y_for_rect);
    word_background_item->setRotation(rad_to_deg(word.baseline.angle));
    d_->text_background_items.push_back(word_background_item);
    d_->text_background_items_group->addToGroup(word_background_item);

    auto parsed_string = parse_utf8_string(word.content);
    auto pos_params = get_character_positioning_params(font_data, parsed_string, word);

    if (pos_params.enable_char_positioning) {
        auto char_x = word_x;
        auto char_y = word_y;

        double curr_x = word.box.x1;

        for (std::size_t i = 0; i < text_utf32.size(); ++i) {
            auto ch = text_utf32.substr(i, 1);
            auto ch_qstring = QString::fromStdU16String(
                        boost::locale::conv::utf_to_utf<char16_t>(ch));

            auto* item = d_->scene->addSimpleText(ch_qstring, font_data.font);
            item->setPos(char_x, char_y);
            item->setTransformOriginPoint(char_x, char_y);
            item->setRotation(rad_to_deg(word.baseline.angle));
            set_tooltip(item, word);
            d_->text_items.push_back(item);
            d_->text_items_group->addToGroup(item);

            const auto& char_box = word.char_boxes[i];
            auto* bounding_box_item = d_->scene->addRect(char_box.x1, char_box.y1,
                                                         char_box.x2 - char_box.x1,
                                                         char_box.y2 - char_box.y1,
                                                         d_->char_bounding_boxes_pen,
                                                         d_->char_bounding_boxes_brush);
            set_tooltip(bounding_box_item, word);
            d_->char_bounding_boxes.push_back(bounding_box_item);
            d_->char_bounding_boxes_group->addToGroup(bounding_box_item);

            auto next_x = (i == text_utf32.size() - 1)
                    ? word.box.x2
                    : word.char_boxes[i + 1].x1;

            char_x += angle_cos * (next_x - curr_x);
            char_y += angle_sin * (next_x - curr_x);

            curr_x = next_x;
        }
    } else {
        auto* item = d_->scene->addText(parsed_string.string, font_data.font);
        item->setPos(word_x, word_y);
        item->setTransformOriginPoint(word_x, word_y);

        // We use full transform instead of calling setScale() and setRotation() because setScale()
        // does not support non-uniform scaling across axes. QTransform::rotateRadians operates
        // on counter-clockwise direction as opposed to QGraphicsItem::setRotation().
        QTransform transform;
        transform.scale(pos_params.h_scale, 1.0);
        transform.rotateRadians(word.baseline.angle);
        item->setTransform(transform);
        set_tooltip(item, word);
        d_->text_items.push_back(item);
        d_->text_items_group->addToGroup(item);
    }
}

void ImageWidgetOcrResultsManager::set_tooltip(QGraphicsItem* item, const OcrWord& word)
{
    item->setToolTip(QString("Confidence: %1").arg(static_cast<unsigned>(word.confidence * 100)));
}

} // namespace sanescan

