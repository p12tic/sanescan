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

#ifndef SANESCAN_GUI_SETTING_SETTING_COMBO_H
#define SANESCAN_GUI_SETTING_SETTING_COMBO_H

#include "setting_widget.h"
#include <QtWidgets/QWidget>
#include <memory>

namespace sanescan {

namespace Ui {
    class SettingCombo;
}

class SettingCombo : public SettingWidget
{
    Q_OBJECT

public:
    explicit SettingCombo(QWidget* parent = nullptr);
    ~SettingCombo() override;

    void set_option_descriptor(const SaneOptionDescriptor& descriptor) override;
    void set_value(const SaneOptionValue& value) override;
    SaneOptionValue get_value() const override;

    void set_enabled(bool enabled) override;

    static bool is_descriptor_supported(const SaneOptionDescriptor& descriptor);

private:
    int find_option_index(const SaneOptionValue& value);

    // either of the following is active
    std::vector<std::string> curr_strings_;
    std::vector<int> curr_int_numbers_;
    std::vector<double> curr_float_numbers_;
    std::vector<std::string> options_;

    SaneOptionDescriptor descriptor_;
    bool descriptor_changed_ = false;
    std::unique_ptr<Ui::SettingCombo> ui_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_SETTING_SETTING_COMBO_H
