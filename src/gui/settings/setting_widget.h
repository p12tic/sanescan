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

#ifndef SANESCAN_GUI_SETTING_SETTING_WIDGET_H
#define SANESCAN_GUI_SETTING_SETTING_WIDGET_H

#include "../../lib/sane_types.h"
#include <QtWidgets/QWidget>
#include <memory>

namespace sanescan {

class SettingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingWidget(QWidget* parent = nullptr);

    virtual void set_option_descriptor(const SaneOptionDescriptor& descriptor) = 0;
    virtual void set_value(const SaneOptionValue& value) = 0;

    /// Returns the current value. If the current value has not yet been set by the user or in case
    /// of unexpected values being entered, empty optional is returned.
    virtual SaneOptionValue get_value() const = 0;

    virtual void set_enabled(bool enabled) = 0;

    static std::unique_ptr<SettingWidget>
        create_widget_for_descriptor(const SaneOptionDescriptor& descriptor);

Q_SIGNALS:
    /// Emitted with the current value returned by get_value() when that one changes as a result of
    /// user action.
    void value_changed(const SaneOptionValue& value);
};

} // namespace sanescan

#endif // SANESCAN_GUI_SETTING_SETTING_COMBO_H
