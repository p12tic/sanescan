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
#include <optional>

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

    /// Enables or disables selection box. In case selection is disabled the current selection
    /// is cleared.
    void set_selection_enabled(bool enabled);

    /// Returns whether selection via clicking and dragging mouse on the widget are enabled.
    bool get_selection_enabled() const;

    /// Sets the visible selection. To disable selection, pass empty optional. If selections
    /// are disabled the call is ignored. Does not emit `selection_changed` signal.
    void set_selection(const std::optional<QRectF>& rect);

    /// Returns the current selection. If there's none, returns empty optional.
    std::optional<QRectF> get_selection() const;

Q_SIGNALS:
    /// Emitted when the selection box is changed. The coordinates are in image coordinates.
    void selection_changed(std::optional<QRectF> rect);

protected:

    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    void setup_selection_items(const QRectF& rect, bool force_resizing_on_first_click);
    void destroy_selection_items();

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_IMAGE_WIDGET_H
