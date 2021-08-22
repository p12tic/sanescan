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

#include "setting_spin_float.h"
#include "ui_setting_spin_float.h"

namespace sanescan {

SettingSpinFloat::SettingSpinFloat(QWidget *parent) :
    QWidget(parent),
    ui_{std::make_unique<Ui::SettingSpinFloat>()}
{
    ui_->setupUi(this);

    connect(ui_->spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [this](double float_value)
    {
        std::vector<double> value;
        value.push_back(float_value);
        Q_EMIT value_changed(value);
    });
}

SettingSpinFloat::~SettingSpinFloat() = default;

void SettingSpinFloat::set_option_descriptor(const SaneOptionDescriptor& descriptor)
{
    if (!(descriptor == descriptor_)) {
        verify_supported_type(descriptor.type);
        descriptor_ = descriptor;
        ui_->label->setText(QString::fromStdString(descriptor.title));
        // FIXME: description

        const auto* constraint = std::get_if<SaneConstraintIntRange>(&descriptor.constraint);
        if (constraint != nullptr) {
            constraint_ = *constraint;
            ui_->spinbox->setRange(constraint_->min, constraint_->max);
            ui_->spinbox->setSingleStep(constraint_->quantization);
        } else {
            constraint_.reset();
        }
    }
    ui_->spinbox->setEnabled(false);
}

void SettingSpinFloat::set_value(const SaneOptionValue& value)
{
    const auto* int_values = std::get_if<std::vector<double>>(&value);
    if (!int_values || int_values->size() != 1) {
        throw std::invalid_argument("Expected float value");
    }

    ui_->spinbox->setValue(int_values->front());
    ui_->spinbox->setEnabled(true);
}

void SettingSpinFloat::set_enabled(bool enabled)
{
    ui_->spinbox->setEnabled(enabled);
}

void SettingSpinFloat::verify_supported_type(SaneValueType type)
{
    if (type != SaneValueType::FLOAT) {
        throw std::invalid_argument("Unsupported value type " +
                                    std::to_string(static_cast<double>(type)));
    }
}

} // namespace sanescan
