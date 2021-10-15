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
#include <QtWidgets/QScrollBar>
#include <QtGui/QWheelEvent>

namespace sanescan {

namespace {
    void adjust_scroll_bar_value(QScrollBar* bar, float scale_mult)
    {
        bar->setValue(int(scale_mult * bar->value() + ((scale_mult - 1) * bar->pageStep() / 2)));
    }
}

struct ImageWidget::Impl {
    QGraphicsScene* scene; // parent widget is an owner
    const QImage* image;
};

ImageWidget::ImageWidget(QWidget *parent) :
    QGraphicsView(parent),
    impl_{std::make_unique<Impl>()}
{
    impl_->scene = new QGraphicsScene(this);
    setScene(impl_->scene);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    set_image_ptr(nullptr);
}

ImageWidget::~ImageWidget() = default;

void ImageWidget::set_image_ptr(const QImage* image)
{
    impl_->image = image;
    if (image) {
        impl_->scene->setSceneRect(0, 0, image->width(), image->height());
        fitInView(impl_->image->rect(), Qt::KeepAspectRatio);
    } else {
        impl_->scene->setSceneRect(0, 0, 300, 400);
    }
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
    if (impl_->image) {
        auto image_rect = rect & sceneRect();
        painter->drawImage(image_rect, *impl_->image, image_rect);
    }
}

} // namespace sanescan
