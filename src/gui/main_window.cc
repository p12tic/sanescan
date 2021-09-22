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

    connect(&engine_timer_, &QTimer::timeout, [this]() { engine_.perform_step(); });
    connect(&engine_, &ScanEngine::devices_refreshed, [this]() { devices_refreshed(); });
    connect(&engine_, &ScanEngine::start_polling, [this]() { engine_timer_.start(1); });
    connect(&engine_, &ScanEngine::stop_polling, [this]() { engine_timer_.stop(); });
    connect(&engine_, &ScanEngine::options_changed, [this]()
    {
        ui_->settings_widget->set_options(engine_.get_option_groups());
    });
    connect(&engine_, &ScanEngine::option_values_changed, [this]()
    {
        ui_->settings_widget->set_option_values(engine_.get_option_values());
    });
    connect(&engine_, &ScanEngine::device_opened, [this]()
    {
        ui_->settings_widget->device_opened();
    });
    connect(&engine_, &ScanEngine::device_closed, [this]()
    {
        if (!open_device_after_close_.empty()) {
            std::string name;
            name.swap(open_device_after_close_);
            engine_.open_device(name);
        }
    });
    connect(&engine_, &ScanEngine::image_updated, [this]()
    {
        ui_->image_area->set_image(engine_.scan_image());
    });
    connect(&engine_, &ScanEngine::scan_finished, [this]()
    {
        scanning_finished();
    });

    connect(ui_->settings_widget, &ScanSettingsWidget::refresh_devices_clicked,
            [this]() { refresh_devices(); });
    connect(ui_->settings_widget, &ScanSettingsWidget::device_selected,
            [this](const std::string& name) { select_device(name); });
    connect(ui_->settings_widget, &ScanSettingsWidget::option_value_changed,
            [this](const auto& name, const auto& value)
    {
        engine_.set_option_value(name, value);
    });
    connect(ui_->settings_widget, &ScanSettingsWidget::scan_started,
            [this]()
    {
        engine_.start_scan();
    });

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

void MainWindow::start_scanning()
{
    engine_.start_scan();
    ui_->stack_settings->setCurrentIndex(STACK_SCANNING);
}

void MainWindow::scanning_finished()
{
    ui_->stack_settings->setCurrentIndex(STACK_SETTINGS);
}

void MainWindow::select_device(const std::string& name)
{
    // we are guaranteed by ui_->settings_widget that device_selected will not be emitted between
    // a previous emission to select_device and a call to device_opened.
    if (engine_.is_device_opened()) {
        engine_.close_device();
        open_device_after_close_ = name;
    } else {
        engine_.open_device(name);
    }
}

} // namespace sanescan
