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

#include "setting_spin.h"
#include "ui_setting_spin.h"

namespace sanescan {

SettingSpin::SettingSpin(QWidget *parent) :
    SettingWidget(parent),
    ui_{std::make_unique<Ui::SettingSpin>()}
{
    ui_->setupUi(this);

    connect(ui_->spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [this](int int_value)
    {
        if (suppress_value_changed_) {
            return;
        }
        Q_EMIT value_changed(get_value());
    });
}

SettingSpin::~SettingSpin() = default;

void SettingSpin::set_option_descriptor(const SaneOptionDescriptor& descriptor)
{
    if (!(descriptor == descriptor_)) {
        if (!is_descriptor_supported(descriptor)) {
            throw std::invalid_argument("SettingSpin: Unsupported option descriptor");
        }

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

void SettingSpin::set_value(const SaneOptionValue& value)
{
    const auto* int_values = std::get_if<std::vector<int>>(&value);
    if (!int_values || int_values->size() != 1) {
        throw std::invalid_argument("Expected integer value");
    }

    suppress_value_changed_ = true;
    ui_->spinbox->setValue(int_values->front());
    suppress_value_changed_ = false;

    ui_->spinbox->setEnabled(true);
}

SaneOptionValue SettingSpin::get_value() const
{
    auto value = ui_->spinbox->value();
    if (constraint_.has_value() && (value < constraint_->min || value > constraint_->max)) {
        return SaneOptionValueNone{};
    }
    return std::vector<int>{ value };
}

bool SettingSpin::is_descriptor_supported(const SaneOptionDescriptor& descriptor)
{
    if (descriptor.type != SaneValueType::INT) {
        return false;
    }

    if (descriptor.size != 1) {
        return false;
    }

    return std::get_if<SaneConstraintIntRange>(&descriptor.constraint) != nullptr ||
            std::get_if<SaneConstraintNone>(&descriptor.constraint) != nullptr;
}

} // namespace sanescan
