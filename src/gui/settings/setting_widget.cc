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

#include "setting_widget.h"
#include "setting_combo.h"
#include "setting_spin.h"
#include "setting_spin_float.h"

namespace sanescan {

namespace {

struct SettingWidgetFactory {
    std::function<bool(const SaneOptionDescriptor&)> is_supported;
    std::function<std::unique_ptr<SettingWidget>()> create;
};

SettingWidgetFactory g_widget_factories[] = {
    { &SettingCombo::is_descriptor_supported,
      [](){ return std::make_unique<SettingCombo>(); } },
    { &SettingSpin::is_descriptor_supported,
      [](){ return std::make_unique<SettingSpin>(); } },
    { &SettingSpinFloat::is_descriptor_supported,
      [](){ return std::make_unique<SettingSpinFloat>(); } },
};

} // namespace

SettingWidget::SettingWidget(QWidget* parent) : QWidget(parent) {}

std::unique_ptr<SettingWidget>
    SettingWidget::create_widget_for_descriptor(const SaneOptionDescriptor& descriptor)
{
    for (const auto& factory : g_widget_factories) {
        if (factory.is_supported(descriptor)) {
            return factory.create();
        }
    }
    return nullptr;
}

} // namespace sanescan
