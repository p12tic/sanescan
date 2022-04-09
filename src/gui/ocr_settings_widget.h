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

#ifndef SANESCAN_GUI_OCR_WIDGET_H
#define SANESCAN_GUI_OCR_WIDGET_H

#include "ocr/ocr_options.h"
#include <QtWidgets/QFrame>
#include <memory>

namespace sanescan {

class OcrSettingsWidget : public QFrame
{
    Q_OBJECT
public:
    explicit OcrSettingsWidget(QWidget* parent);
    ~OcrSettingsWidget() override;

    void set_options(const OcrOptions& options);

public: Q_SIGNALS:
    void options_changed(const OcrOptions& options);

private:
    void options_changed_impl();

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_OCR_WIDGET_H
