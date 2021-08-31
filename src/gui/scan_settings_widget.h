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

#ifndef SANESCAN_GUI_SCAN_SETTINGS_H
#define SANESCAN_GUI_SCAN_SETTINGS_H

#include "../lib/sane_types.h"
#include "settings/setting_widget.h"
#include <QtWidgets/QGridLayout>
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

    void set_current_devices(const std::vector<SaneDeviceInfo>& devices);
    void device_opened();
    void set_options(const std::vector<SaneOptionGroupDestriptor>& descriptors);
    void set_option_values(const std::map<std::string, SaneOptionValue>& values);

Q_SIGNALS:
    void refresh_devices_clicked();

    /// Freezes the device selector until device_opened() is called
    void device_selected(const std::string& name);

private:
    void device_selected_impl(int index);
    void refresh_widgets();
    void clear_layout();

    std::vector<SaneDeviceInfo> devices_;

    bool waiting_for_device_opened_ = false;
    std::vector<SaneOptionGroupDestriptor> curr_group_descriptors_;

    // Layout widget is not owned, the owner is `this`. Note that we terminate the relationship
    // by deleting the widget.
    QGridLayout* layout_ = nullptr;

    // Widgets are not owned, the owner is layout_
    bool setting_widgets_need_initial_values_ = false;
    std::map<std::string, SettingWidget*> setting_widgets_;

    std::unique_ptr<Ui::ScanSettingsWidget> ui_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_SCAN_SETTINGS_H
