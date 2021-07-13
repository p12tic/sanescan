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

#ifndef SANESCAN_IMAGE_WIDGET_H
#define SANESCAN_IMAGE_WIDGET_H

#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

namespace sanescan {

class ImageWidget : public QScrollArea
{
    Q_OBJECT

public:
    explicit ImageWidget(QWidget *parent = nullptr);
    ~ImageWidget() override;

    void wheelEvent(QWheelEvent* event) override;

    void set_image(const QImage& image);
    void rescale_by(float scale_mult);

private:
    QLabel* image_ = nullptr;
    float scale_ = 1.0f;
};

} // namespace sanescan

#endif // SANESCAN_IMAGE_WIDGET_H
