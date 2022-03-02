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
    connect(ui_->b_scan, &QPushButton::clicked,
            [this]() { Q_EMIT scan_started(); });
    connect(ui_->cb_scanners, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int index) { device_selected_impl(index); });
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
    if (!devices_.empty()) {
        ui_->cb_scanners->setCurrentIndex(0);
    }
}

void ScanSettingsWidget::device_opened()
{
    if (waiting_for_device_opened_) {
        waiting_for_device_opened_ = false;
        ui_->cb_scanners->setEnabled(true);
    }
}

void ScanSettingsWidget::set_options(const std::vector<SaneOptionGroupDestriptor>& descriptors)
{
    if (curr_group_descriptors_ == descriptors)
        return;

    curr_group_descriptors_ = descriptors;
    refresh_widgets();
}

void ScanSettingsWidget::set_option_values(const std::map<std::string, SaneOptionValue>& values)
{
    for (const auto& [name, value] : values) {
        set_option_value(name, value);
    }
    setting_widgets_need_initial_values_ = false;
}

void ScanSettingsWidget::set_option_value(const std::string& name, const SaneOptionValue& value)
{
    auto it = setting_widgets_.find(name);
    if (it == setting_widgets_.end()) {
        return;
    }

    auto* setting_widget = it->second;
    auto curr_value = setting_widget->get_value();
    if (curr_value == value && !setting_widgets_need_initial_values_) {
        return;
    }

    if (std::get_if<SaneOptionValueNone>(&value)) {
        return;  // FIXME: we need a way to show currently not set options
    }

    setting_widget->set_value(value);
}

void ScanSettingsWidget::device_selected_impl(int index)
{
    if (index < 0 || index >= devices_.size())
        return;

    clear_layout();

    ui_->cb_scanners->setEnabled(false);
    waiting_for_device_opened_ = true;

    Q_EMIT device_selected(devices_[index].name);
}

void ScanSettingsWidget::refresh_widgets()
{
    clear_layout();
    setting_widgets_need_initial_values_ = true;

    int curr_row = 0;
    for (const auto& group : curr_group_descriptors_) {
        // TODO: don't ignore groups
        for (const auto& option_descriptor : group.options) {
            auto widget = SettingWidget::create_widget_for_descriptor(option_descriptor);
            if (!widget) {
                continue;
            }
            auto* not_owned_widget = widget.release();
            layout_->addWidget(not_owned_widget, curr_row++, 0); // takes ownership
            not_owned_widget->set_option_descriptor(option_descriptor);
            connect(not_owned_widget, &SettingWidget::value_changed,
                    [this, name=option_descriptor.name](const auto& new_value)
            {
                Q_EMIT option_value_changed(name, new_value);
            });

            setting_widgets_.emplace(option_descriptor.name, not_owned_widget);
        }
    }
}

void ScanSettingsWidget::clear_layout()
{
    if (layout_) {
        for (const auto& [_, widget] : setting_widgets_) {
            delete widget;
        }
        setting_widgets_.clear();
        delete layout_;
    }
    layout_ = new QGridLayout();
    ui_->layout->insertLayout(3, layout_);
}

} // namespace sanescan
