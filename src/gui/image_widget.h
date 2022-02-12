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

#ifndef SANESCAN_GUI_IMAGE_WIDGET_H
#define SANESCAN_GUI_IMAGE_WIDGET_H

#include <QtWidgets/QGraphicsView>
#include <memory>

namespace sanescan {

class ImageWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageWidget(QWidget *parent = nullptr);
    ~ImageWidget() override;

    /// Note that QImage uses reference semantics, so internally the widget refers to the under
    /// lying data of the argument even after the call.
    void set_image(const QImage& image);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_IMAGE_WIDGET_H
