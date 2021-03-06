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

#include "scan_page.h"
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

    static constexpr int TAB_SCANNING = 0;
    static constexpr int TAB_OCR = 1;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void present_about_dialog();

private:
    void start_scanning(ScanType type);

    QImage get_page_thumbnail(const ScanPage& page);
    QImage get_page_image(const ScanPage& page);

    void switch_to_page(unsigned page_index);

    void update_selection_to_settings();
    void image_area_selection_changed(const std::optional<QRectF>& rect);
    void update_ocr_tab_to_settings();
    void update_ocr_results_manager();

    void save_all_pages();
    void save_all_pages_with_ocr();
    void save_current_page();

    bool warn_if_is_unsupported_save_path(const std::string& path);
    bool is_supported_save_path(const std::string& path);

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_MAIN_WINDOW_H
