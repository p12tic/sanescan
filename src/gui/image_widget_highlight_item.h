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

#ifndef SANESCAN_GUI_IMAGE_WIDGET_HIGHLIGHT_ITEM_H
#define SANESCAN_GUI_IMAGE_WIDGET_HIGHLIGHT_ITEM_H

#include <QtWidgets/QGraphicsItem>
#include <memory>

namespace sanescan {

// Paints whole graphics scene with specific color and opacity except a given rectangle
class ImageWidgetHighlightItem : public QGraphicsItem {
public:
    explicit ImageWidgetHighlightItem(const QRectF& rect, const QRectF& highlight_rect,
                                      const QColor& color);
    ~ImageWidgetHighlightItem() override;

    void set_rect(const QRectF& rect);
    const QRectF& rect() const;

    void set_highlight_rect(const QRectF& rect);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

private:
    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_IMAGE_WIDGET_HIGHLIGHT_ITEM_H
