/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Monika Kairaityte <monika@kibit.lt>

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

#include "image_widget.h"
#include "image_widget_highlight_item.h"
#include "image_widget_selection_item.h"
#include <QtWidgets/QScrollBar>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QGraphicsRectItem>

namespace sanescan {

namespace {
    void adjust_scroll_bar_value(QScrollBar* bar, float scale_mult)
    {
        bar->setValue(int(scale_mult * bar->value() + ((scale_mult - 1) * bar->pageStep() / 2)));
    }
}

struct ImageWidget::Private {
    QGraphicsScene* scene = nullptr; // parent widget is an owner
    QImage image;
    bool selection_enabled = false;

    ImageWidgetHighlightItem* highlight_item = nullptr;
    ImageWidgetSelectionItem* selection_item = nullptr;
};

ImageWidget::ImageWidget(QWidget *parent) :
    QGraphicsView(parent),
    d_{std::make_unique<Private>()}
{
    d_->scene = new QGraphicsScene(this);
    setScene(d_->scene);
}

ImageWidget::~ImageWidget() = default;

void ImageWidget::set_image(const QImage& image)
{
    d_->image = image;
    if (!image.isNull()) {
        d_->scene->setSceneRect(0, 0, image.width(), image.height());
        fitInView(d_->image.rect(), Qt::KeepAspectRatio);
    } else {
        d_->scene->setSceneRect(0, 0, 300, 400);
    }
}

void ImageWidget::set_selection_enabled(bool enabled)
{
    if (d_->selection_enabled == enabled) {
        return;
    }
    d_->selection_enabled = enabled;
    if (!enabled && d_->selection_item != nullptr) {
        destroy_selection_items();
    }
}

bool ImageWidget::get_selection_enabled() const
{
    return d_->selection_enabled;
}

void ImageWidget::set_selection(const std::optional<QRectF>& rect)
{
    if (!d_->selection_enabled) {
        return;
    }
    if (rect.has_value()) {
        if (d_->selection_item == nullptr) {
            setup_selection_items(rect.value(), false);
        } else {
            d_->selection_item->set_rect(rect.value());
            d_->highlight_item->set_highlight_rect(rect.value());
        }

    } else {
        if (d_->selection_item != nullptr) {
            destroy_selection_items();
        }
    }
}

std::optional<QRectF> ImageWidget::get_selection() const
{
    if (d_->selection_item == nullptr) {
        return {};
    }
    return d_->selection_item->rect();
}

void ImageWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        event->accept();
        if (event->angleDelta().y() == 0)
            return;

        // FIXME: this will work horrible with hi-res scrolling (too frequent updates)
        float scaled_delta = (event->angleDelta().y() / 120.0f) * 0.1f;
        float new_scale = scaled_delta >= 0 ? (1 + scaled_delta) : 1 / (1 - scaled_delta);
        scale(new_scale, new_scale);
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void ImageWidget::drawBackground(QPainter* painter, const QRectF& rect)
{
    QColor background_color{0xa0, 0xa0, 0xa0};

    if (!d_->image.isNull()) {
        auto image_rect = rect & sceneRect();
        if (image_rect != rect) {
            painter->fillRect(rect, background_color);
        }
        painter->drawImage(image_rect, d_->image, image_rect);
    } else {
        painter->fillRect(rect, background_color);
    }
}

void ImageWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if any other view item accepted the event and if not then we probably can create
        // a selection area.
        event->ignore();
        QGraphicsView::mousePressEvent(event);

        if (!event->isAccepted()) {
            if (!d_->selection_enabled || d_->selection_item != nullptr) {
                return;
            }

            // Attempt to create a new selection item and if we created one and click again to
            // activate resizing of it.
            auto event_pos_in_scene = mapToScene(event->pos());
            auto image_rect = sceneRect();
            if (!image_rect.contains(event_pos_in_scene)) {
                return;
            }

            setup_selection_items(QRectF(event_pos_in_scene, QSizeF(10, 10)), true);

            QGraphicsView::mousePressEvent(event);
        }
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void ImageWidget::setup_selection_items(const QRectF& rect, bool force_resizing_on_first_click)
{
    auto image_rect = sceneRect();
    d_->selection_item = new ImageWidgetSelectionItem(image_rect, rect,
                                                      force_resizing_on_first_click);
    d_->scene->addItem(d_->selection_item);

    QColor highlight_color = Qt::black;
    highlight_color.setAlpha(50);

    d_->highlight_item = new ImageWidgetHighlightItem(image_rect, rect, highlight_color);
    d_->selection_item->set_on_moved([=, this](const QRectF& rect)
    {
        d_->highlight_item->set_highlight_rect(rect);
        Q_EMIT selection_changed(rect);
    });

    d_->scene->addItem(d_->highlight_item);
}

void ImageWidget::destroy_selection_items()
{
    d_->scene->removeItem(d_->selection_item);
    d_->scene->removeItem(d_->highlight_item);
    delete d_->selection_item;
    delete d_->highlight_item;
    d_->selection_item = nullptr;
    d_->highlight_item = nullptr;
}

} // namespace sanescan
