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

#include "image_widget_highlight_item.h"
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>

namespace sanescan {

struct ImageWidgetHighlightItem::Private {
    QRectF rect;
    QRectF highlight_rect;
    QBrush brush;
};

ImageWidgetHighlightItem::ImageWidgetHighlightItem(const QRectF& rect, const QRectF& highlight_rect,
                                                   const QColor& color) :
    d_{std::make_unique<Private>()}
{
    d_->rect = rect;
    d_->highlight_rect = highlight_rect;
    d_->brush.setColor(color);
    d_->brush.setStyle(Qt::SolidPattern);
}

ImageWidgetHighlightItem::~ImageWidgetHighlightItem() = default;

void ImageWidgetHighlightItem::set_rect(const QRectF& rect)
{
    if (d_->rect == rect) {
        return;
    }
    prepareGeometryChange();
    d_->rect = rect;
    update();
}

const QRectF& ImageWidgetHighlightItem::rect() const
{
    return d_->rect;
}

void ImageWidgetHighlightItem::set_highlight_rect(const QRectF& rect)
{
    d_->highlight_rect = rect;
}

void ImageWidgetHighlightItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                                     QWidget* widget)
{
    painter->save();

    painter->setBrush(d_->brush);
    painter->setPen(Qt::NoPen);

    /* We draw 4 rectangles: one to the top of rect, two to the sides and one to the bottom.

        d_->rect.topLeft()                                     d_->rect.topRight
          ┌──────────────────────────────────────────────────────┐
          │                                                      │
          │                     rect_top                         │
          │                                                      │
          ├─────────────┬────────────────────────┬───────────────┤
          │             │                        │               │
          │             │                        │               │
          │  rect_left  │  d_->highlight_rect    │   rect_right  │
          │             │                        │               │
          │             │                        │               │
          ├─────────────┴────────────────────────┴───────────────┤
          │                                                      │
          │                     rect_bottom                      │
          │                                                      │
          └──────────────────────────────────────────────────────┘
        d_->rect.bottomLeft                                    d_->rect.bottomRight()
    */

    auto rect_top = QRectF(d_->rect.topLeft(),
                           QPointF(d_->rect.right(), d_->highlight_rect.top()));
    auto rect_left = QRectF(QPointF(d_->rect.left(), d_->highlight_rect.top()),
                            d_->highlight_rect.bottomLeft());
    auto rect_right = QRectF(d_->highlight_rect.topRight(),
                             QPointF(d_->rect.right(), d_->highlight_rect.bottom()));
    auto rect_bottom = QRectF(QPointF(d_->rect.left(), d_->highlight_rect.bottom()),
                              d_->rect.bottomRight());

    if (rect_top.isValid()) {
        painter->drawRect(rect_top);
    }
    if (rect_left.isValid()) {
        painter->drawRect(rect_left);
    }
    if (rect_right.isValid()) {
        painter->drawRect(rect_right);
    }
    if (rect_bottom.isValid()) {
        painter->drawRect(rect_bottom);
    }

    painter->restore();
}

QRectF ImageWidgetHighlightItem::boundingRect() const
{
    return d_->rect;
}

QPainterPath ImageWidgetHighlightItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

} // namespace sanescan
