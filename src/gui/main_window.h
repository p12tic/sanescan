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

#ifndef SANESCAN_GUI_MAIN_WINDOW_H
#define SANESCAN_GUI_MAIN_WINDOW_H

#include "scan_document.h"
#include "../lib/sane_types.h"
#include <QtWidgets/QMainWindow>
#include <memory>

namespace sanescan {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static constexpr int STACK_LOADING = 0;
    static constexpr int STACK_SETTINGS = 1;
    static constexpr int STACK_SCANNING = 2;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void present_about_dialog();

private:
    void start_scanning();

    QImage get_document_thumbnail(const ScanDocument& document);

    void switch_to_document(unsigned doc_index);

    void maybe_update_selection_after_setting_change(const std::string& name,
                                                     const SaneOptionValue& value);
    void image_area_selection_changed(const std::optional<QRectF>& rect);

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_MAIN_WINDOW_H
