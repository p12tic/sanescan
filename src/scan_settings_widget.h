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

#ifndef SANESCAN_SCAN_SETTINGS_H
#define SANESCAN_SCAN_SETTINGS_H

#include <QtWidgets/QFrame>
#include <memory>

namespace sanescan {

namespace Ui {
    class ScanSettingsWidget;
}

class ScanSettingsWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ScanSettingsWidget(QWidget *parent = nullptr);
    ~ScanSettingsWidget() override;

private:
    std::unique_ptr<Ui::ScanSettingsWidget> ui_;
};

} // namespace sanescan

#endif // SANESCAN_SCAN_SETTINGS_H
