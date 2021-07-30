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

#include "main_window.h"
#include "about_dialog.h"
#include "image_widget.h"
#include "scan_settings_widget.h"
#include "ui_main_window.h"

namespace sanescan {

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui_{std::make_unique<Ui::MainWindow>()}
{
    ui_->setupUi(this);

    connect(ui_->action_about, &QAction::triggered, [this](){ present_about_dialog(); });

    ui_->image_area->set_image(QImage("test.jpeg"));

    connect(&engine_timer_, &QTimer::timeout, [this]() { engine_.perform_step(); });
    connect(&engine_, &ScanEngine::devices_refreshed, [this]() { devices_refreshed(); });
    connect(&engine_, &ScanEngine::start_polling, [this]() { engine_timer_.start(1); });
    connect(&engine_, &ScanEngine::stop_polling, [this]() { engine_timer_.stop(); });
    connect(ui_->settings_widget, &ScanSettingsWidget::refresh_devices_clicked,
            [this]() { refresh_devices(); });

    refresh_devices();
}

MainWindow::~MainWindow() = default;

void MainWindow::present_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::refresh_devices()
{
    ui_->stack_settings->setCurrentIndex(STACK_LOADING);
    engine_.refresh_devices();
}

void MainWindow::devices_refreshed()
{
    ui_->stack_settings->setCurrentIndex(STACK_SETTINGS);
    ui_->settings_widget->set_current_devices(engine_.current_devices());
}

} // namespace sanescan
