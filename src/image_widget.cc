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

ImageWidget::ImageWidget(QWidget *parent) :
    QScrollArea(parent)
{
    image_ = new QLabel();
    image_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    image_->setScaledContents(true);
    setWidget(image_);
    setVisible(false);
}

ImageWidget::~ImageWidget() = default;

void ImageWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        event->accept();
        if (event->orientation() != Qt::Vertical)
            return;

        // FIXME: this will work horrible with hi-res scrolling (too frequent updates)
        float scaled_delta = (event->angleDelta().y() / 120.0f) * 0.1f;
        float scale = scaled_delta >= 0 ? (1 + scaled_delta) : 1 / (1 - scaled_delta);
        rescale_by(scale);
    } else {
        QScrollArea::wheelEvent(event);
    }
}

void ImageWidget::set_image(const QImage& image)
{
    image_->setPixmap(QPixmap::fromImage(image));
    image_->adjustSize();
    scale_ = 1.0;
    setVisible(true);
}


void ImageWidget::rescale_by(float scale_mult)
{
    scale_ *= scale_mult;
    image_->resize(scale_ * image_->pixmap()->size());

    adjust_scroll_bar_value(horizontalScrollBar(), scale_);
    adjust_scroll_bar_value(verticalScrollBar(), scale_);
}

} // namespace sanescan
