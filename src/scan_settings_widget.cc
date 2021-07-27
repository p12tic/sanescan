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

#include "scan_settings_widget.h"
#include "ui_scan_settings_widget.h"

namespace sanescan {


ScanSettingsWidget::ScanSettingsWidget(QWidget *parent) :
    QFrame(parent),
    ui_{std::make_unique<Ui::ScanSettingsWidget>()}
{
    ui_->setupUi(this);

    connect(ui_->b_refresh_devices, &QPushButton::clicked,
            [this]() { Q_EMIT refresh_devices_clicked(); });
}

ScanSettingsWidget::~ScanSettingsWidget() = default;

void ScanSettingsWidget::set_current_devices(const std::vector<SaneDeviceInfo>& devices)
{
    devices_ = devices;
    ui_->cb_scanners->clear();
    for (const auto& dev : devices_) {
        ui_->cb_scanners->addItem(QString::fromStdString(dev.vendor + " " + dev.model +
                                                         " (" + dev.name + ")"));
    }
}

} // namespace sanescan
