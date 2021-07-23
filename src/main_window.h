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

#ifndef SANESCAN_MAIN_WINDOW_H
#define SANESCAN_MAIN_WINDOW_H

#include "scan_engine.h"
#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include <memory>

namespace sanescan {

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static constexpr int STACK_LOADING = 0;
    static constexpr int STACK_SETTINGS = 1;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void present_about_dialog();

private:
    void devices_refreshed();

    std::unique_ptr<Ui::MainWindow> ui_;
    ScanEngine engine_;
    QTimer engine_timer_;
};

} // namespace sanescan

#endif
