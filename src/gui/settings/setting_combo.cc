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

#include "setting_combo.h"
#include "ui_setting_combo.h"

namespace sanescan {

namespace {

template<class T>
std::vector<std::string> to_string_vector(const std::vector<T>& array)
{
    std::vector<std::string> res;
    res.reserve(array.size());
    for (const auto& item : array) {
        res.push_back(std::to_string(item));
    }
    return res;
}

template<class T>
int find_option_index_single_impl(const std::vector<T>& values, const T& got_value,
                                  SaneValueType option_type, SaneValueType expected_option_type)
{
    if (option_type != expected_option_type) {
        throw std::invalid_argument("Got invalid value for option. Expected: " +
                                    std::to_string(static_cast<int>(option_type)) +
                                    " got: " +
                                    std::to_string(static_cast<int>(expected_option_type)));
    }
    auto it = std::find(values.begin(), values.end(), got_value);
    if (it == values.end())
        return -1;
    return std::distance(values.begin(), it);
}

template<class T>
int find_option_index_impl(const std::vector<T>& values, const std::vector<T>& got_values,
                           SaneValueType option_type, SaneValueType expected_option_type)
{
    if (got_values.size() != 1) {
        throw std::invalid_argument("Got value of invalid size: " +
                                    std::to_string(got_values.size()));
    }
    return find_option_index_single_impl(values, got_values.front(),
                                         option_type, expected_option_type);
}

} // namespace

SettingCombo::SettingCombo(QWidget *parent) :
    SettingWidget(parent),
    ui_{std::make_unique<Ui::SettingCombo>()}
{
    ui_->setupUi(this);

    // Note that only activated() signal will react to user action, but not programmatically
    // setting the value. This is important because we may reset the values after each
    // value change.
    connect(ui_->combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [this](int) { Q_EMIT value_changed(get_value()); });
}

SettingCombo::~SettingCombo() = default;

void SettingCombo::set_option_descriptor(const SaneOptionDescriptor& descriptor)
{
    if (!(descriptor == descriptor_)) {
        if (!is_descriptor_supported(descriptor)) {
            throw std::invalid_argument("SettingCombo: Unsupported option descriptor");
        }

        descriptor_ = descriptor;
        ui_->label->setText(QString::fromStdString(descriptor.title));
        // FIXME: description
        ui_->combobox->clear();

        const auto* int_numbers = std::get_if<SaneConstraintIntList>(&descriptor.constraint);
        if (int_numbers != nullptr) {
            curr_int_numbers_ = int_numbers->numbers;
            options_ = to_string_vector(curr_int_numbers_);
        }

        const auto* float_numbers = std::get_if<SaneConstraintFloatList>(&descriptor.constraint);
        if (float_numbers != nullptr) {
            curr_float_numbers_ = float_numbers->numbers;
            options_ = to_string_vector(curr_float_numbers_);
        }

        const auto* strings = std::get_if<SaneConstraintStringList>(&descriptor.constraint);
        if (strings != nullptr) {
            curr_strings_ = strings->strings;
            options_ = curr_strings_;
        }
        descriptor_changed_ = true;
    }

    ui_->combobox->setEnabled(false);
}

void SettingCombo::set_value(const SaneOptionValue& value)
{
    if (descriptor_changed_) {
        descriptor_changed_ = false;
        ui_->combobox->clear();
        for (const auto& option : options_) {
            ui_->combobox->addItem(QString::fromStdString(option));
        }
    }

    ui_->combobox->setCurrentIndex(find_option_index(value));
    ui_->combobox->setEnabled(true);
}

SaneOptionValue SettingCombo::get_value() const
{
    int index = ui_->combobox->currentIndex();
    if (index < 0) {
        return SaneOptionValueNone{};
    }

    switch (descriptor_.type) {
        case SaneValueType::FLOAT:
            return std::vector<double>{ curr_float_numbers_.at(index) };
        case SaneValueType::INT:
            return std::vector<int> { curr_int_numbers_.at(index) };
        case SaneValueType::STRING:
            return curr_strings_.at(index);
        default:
            return SaneOptionValueNone{};
    }
}

bool SettingCombo::is_descriptor_supported(const SaneOptionDescriptor& descriptor)
{
    switch (descriptor.type) {
        case SaneValueType::FLOAT:
            return std::get_if<SaneConstraintFloatList>(&descriptor.constraint) != nullptr &&
                    descriptor.size == 1;
        case SaneValueType::INT:
            return std::get_if<SaneConstraintIntList>(&descriptor.constraint) != nullptr &&
                    descriptor.size == 1;
        case SaneValueType::STRING:
            return std::get_if<SaneConstraintStringList>(&descriptor.constraint) != nullptr;
        default:
            return false;
    }
}

int SettingCombo::find_option_index(const SaneOptionValue& value)
{
    const auto* string_value = std::get_if<std::string>(&value);
    if (string_value) {
        return find_option_index_single_impl(curr_strings_, *string_value,
                                             descriptor_.type, SaneValueType::STRING);
    }
    const auto* int_values = std::get_if<std::vector<int>>(&value);
    if (int_values) {
        return find_option_index_impl(curr_int_numbers_, *int_values,
                                      descriptor_.type, SaneValueType::INT);
    }
    const auto* float_values = std::get_if<std::vector<double>>(&value);
    if (float_values) {
        return find_option_index_impl(curr_float_numbers_, *float_values,
                                      descriptor_.type, SaneValueType::FLOAT);
    }
    throw std::invalid_argument("Unsupported value type " + std::to_string(value.index()));
}

} // namespace sanescan
