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

#include "image_widget_selection_item.h"
#include <QtGui/QCursor>
#include <QtGui/QPen>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <cmath>

namespace sanescan {

constexpr double CURSOR_ACTIVATION_PIXELS = 10;

namespace {
    enum class HoverType {
        MOVE = 0,
        RESIZE_LEFT,
        RESIZE_RIGHT,
        RESIZE_TOP,
        RESIZE_BOTTOM,
        RESIZE_TOP_LEFT,
        RESIZE_TOP_RIGHT,
        RESIZE_BOTTOM_LEFT,
        RESIZE_BOTTOM_RIGHT
    };

    HoverType get_hover_type(const QRectF& r, const QPointF& pos)
    {
        // TODO: handle scale
        bool is_near_top = std::abs(r.top() - pos.y()) <= CURSOR_ACTIVATION_PIXELS;
        bool is_near_bottom = std::abs(r.bottom() - pos.y()) <= CURSOR_ACTIVATION_PIXELS;
        bool is_near_left = std::abs(r.left() - pos.x()) <= CURSOR_ACTIVATION_PIXELS;
        bool is_near_right = std::abs(r.right() - pos.x()) <= CURSOR_ACTIVATION_PIXELS;
        bool is_inside = r.contains(pos);

        if (((is_near_top && is_near_bottom) || (is_near_right && is_near_left)) && is_inside) {
            // Handle the case when the rectangle is so small that its whole area is near the
            // bounds. Moving the cursor within the rectangle area should still be able to move it.
            return HoverType::MOVE;
        } else if (is_near_top && is_near_left) {
            return HoverType::RESIZE_TOP_LEFT;
        } else if (is_near_top && is_near_right) {
            return HoverType::RESIZE_TOP_RIGHT;
        } else if (is_near_bottom && is_near_left) {
            return HoverType::RESIZE_BOTTOM_LEFT;
        } else if (is_near_bottom && is_near_right) {
            return HoverType::RESIZE_BOTTOM_RIGHT;
        } else if (is_near_top) {
            return HoverType::RESIZE_TOP;
        } else if (is_near_bottom) {
            return HoverType::RESIZE_BOTTOM;
        } else if (is_near_left) {
            return HoverType::RESIZE_LEFT;
        } else if (is_near_right) {
            return HoverType::RESIZE_RIGHT;
        } else {
            return HoverType::MOVE;
        }
    }

    Qt::CursorShape get_cursor_shape(HoverType type)
    {
        switch (type) {
            case HoverType::MOVE: return Qt::SizeAllCursor;
            case HoverType::RESIZE_TOP_LEFT: return Qt::SizeFDiagCursor;
            case HoverType::RESIZE_BOTTOM_RIGHT: return Qt::SizeFDiagCursor;
            case HoverType::RESIZE_TOP_RIGHT: return Qt::SizeBDiagCursor;
            case HoverType::RESIZE_BOTTOM_LEFT: return Qt::SizeBDiagCursor;
            case HoverType::RESIZE_TOP: return Qt::SizeVerCursor;
            case HoverType::RESIZE_BOTTOM: return Qt::SizeVerCursor;
            case HoverType::RESIZE_LEFT: return Qt::SizeHorCursor;
            case HoverType::RESIZE_RIGHT: return Qt::SizeHorCursor;
            default:
                return Qt::SizeAllCursor;
        }
    }

} // namespace

struct ImageWidgetSelectionItem::Private {
    QRectF rect;
    QRectF move_bounds_rect;
    QPen bounds_pen;

    bool waiting_for_first_click = true;
    QRectF last_press_moving_rect;
    QPointF last_press_moving_point;
    QPointF last_press_static_point;
    HoverType last_press_hover_type{};

    std::function<void(const QRectF&)> moved_callback;
};

ImageWidgetSelectionItem::ImageWidgetSelectionItem(const QRectF& move_bounds, const QRectF& rect) :
    d_{std::make_unique<Private>()}
{
    d_->move_bounds_rect = move_bounds;
    set_rect(rect);

    // In case this item is created and added when a button has already been clicked we assume
    // that we've been given a newly created small item and we need to allow to resize it.
    d_->last_press_moving_point = rect.bottomRight();
    d_->last_press_static_point = rect.topLeft();
    d_->last_press_hover_type = HoverType::RESIZE_BOTTOM_RIGHT;

    d_->bounds_pen.setWidth(0);
    d_->bounds_pen.setColor(Qt::black);
    d_->bounds_pen.setStyle(Qt::SolidLine);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

ImageWidgetSelectionItem::~ImageWidgetSelectionItem() = default;

void ImageWidgetSelectionItem::set_on_moved(const std::function<void(const QRectF&)>& cb)
{
    d_->moved_callback = cb;
}

void ImageWidgetSelectionItem::set_rect(const QRectF& rect)
{
    auto clipped_rect = rect & d_->move_bounds_rect;
    if (d_->rect == clipped_rect) {
        return;
    }
    prepareGeometryChange();
    d_->rect = clipped_rect;
    update();
    if (d_->moved_callback) {
        d_->moved_callback(clipped_rect);
    }
}

const QRectF& ImageWidgetSelectionItem::rect() const
{
    return d_->rect;
}

void ImageWidgetSelectionItem::set_move_bounds(const QRectF& move_bounds)
{
    d_->move_bounds_rect = move_bounds;
}

void ImageWidgetSelectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                                     QWidget* widget)
{
    painter->save();

    painter->setPen(d_->bounds_pen);
    painter->drawRect(rect());

    painter->restore();
}

QRectF ImageWidgetSelectionItem::boundingRect() const
{
    // TODO: handle scale
    return rect().adjusted(-CURSOR_ACTIVATION_PIXELS, -CURSOR_ACTIVATION_PIXELS,
                           CURSOR_ACTIVATION_PIXELS, CURSOR_ACTIVATION_PIXELS);
}

QPainterPath ImageWidgetSelectionItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

void ImageWidgetSelectionItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    // TODO: handle scale
    setCursor(get_cursor_shape(get_hover_type(rect(), event->pos())));
}

void ImageWidgetSelectionItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (d_->waiting_for_first_click) {
        // The selection item is activated by a fake click the first time, so we reuse the
        // initial data set in the constructor.
        d_->waiting_for_first_click = false;
        event->accept();
        return;
    }

    d_->last_press_hover_type = get_hover_type(rect(), event->pos());
    setCursor(get_cursor_shape(d_->last_press_hover_type));

    switch (d_->last_press_hover_type) {
        case HoverType::MOVE:
            d_->last_press_moving_rect = rect();
            break;
        case HoverType::RESIZE_TOP:
        case HoverType::RESIZE_LEFT:
        case HoverType::RESIZE_TOP_LEFT:
            d_->last_press_moving_point = rect().topLeft();
            d_->last_press_static_point = rect().bottomRight();
            break;
        case HoverType::RESIZE_RIGHT:
        case HoverType::RESIZE_TOP_RIGHT:
            d_->last_press_moving_point = rect().topRight();
            d_->last_press_static_point = rect().bottomLeft();
            break;
        case HoverType::RESIZE_BOTTOM:
        case HoverType::RESIZE_BOTTOM_RIGHT:
            d_->last_press_moving_point = rect().bottomRight();
            d_->last_press_static_point = rect().topLeft();
            break;
        case HoverType::RESIZE_BOTTOM_LEFT:
            d_->last_press_moving_point = rect().bottomLeft();
            d_->last_press_static_point = rect().topRight();
            break;
    }

    event->accept();
}

void ImageWidgetSelectionItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) != Qt::LeftButton) {
        return;
    }

    auto mouse_pos_diff = event->pos() - event->buttonDownPos(Qt::LeftButton);

    // TODO: handle scale
    switch (d_->last_press_hover_type) {
        case HoverType::MOVE:
            set_rect(d_->last_press_moving_rect.translated(mouse_pos_diff));
            return;
        case HoverType::RESIZE_TOP_RIGHT:
        case HoverType::RESIZE_TOP_LEFT:
        case HoverType::RESIZE_BOTTOM_RIGHT:
        case HoverType::RESIZE_BOTTOM_LEFT:
            set_rect(QRectF(d_->last_press_moving_point + mouse_pos_diff,
                            d_->last_press_static_point).normalized());
            break;
        case HoverType::RESIZE_TOP:
        case HoverType::RESIZE_BOTTOM:
            set_rect(QRectF(d_->last_press_moving_point + QPointF(0, mouse_pos_diff.y()),
                            d_->last_press_static_point).normalized());
            break;
        case HoverType::RESIZE_RIGHT:
        case HoverType::RESIZE_LEFT:
            set_rect(QRectF(d_->last_press_moving_point + QPointF(mouse_pos_diff.x(), 0),
                            d_->last_press_static_point).normalized());
            break;
    }
}

} // namespace sanescan
