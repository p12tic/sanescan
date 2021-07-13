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
}

ScanSettingsWidget::~ScanSettingsWidget() = default;

} // namespace sanescan
