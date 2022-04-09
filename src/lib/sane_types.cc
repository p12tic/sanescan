/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Povilas Kanapickas <povilas@radix.lt>

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

#include "sane_types.h"
#include <sane/sane.h>
#include <iostream>

namespace sanescan {

namespace {
    static_assert(static_cast<int>(SaneUnit::NONE) == SANE_UNIT_NONE);
    static_assert(static_cast<int>(SaneUnit::PIXEL) == SANE_UNIT_PIXEL);
    static_assert(static_cast<int>(SaneUnit::BIT) == SANE_UNIT_BIT);
    static_assert(static_cast<int>(SaneUnit::MM) == SANE_UNIT_MM);
    static_assert(static_cast<int>(SaneUnit::DPI) == SANE_UNIT_DPI);
    static_assert(static_cast<int>(SaneUnit::PERCENT) == SANE_UNIT_PERCENT);
    static_assert(static_cast<int>(SaneUnit::MICROSECOND) == SANE_UNIT_MICROSECOND);

    struct SaneUnitDescription
    {
        SaneUnit unit;
        const char* lowercase_desc;
        const char* uppercase_desc;
    };

    SaneUnitDescription g_sane_unit_descriptions[] = {
        {   SaneUnit::NONE, "none", "None" },
        {   SaneUnit::PIXEL, "pixels", "Pixels" },
        {   SaneUnit::BIT, "bits", "Bits" },
        {   SaneUnit::MM, "milimeters", "Milimeters" },
        {   SaneUnit::DPI, "DPI", "DPI" },
        {   SaneUnit::PERCENT, "percent", "Percent" },
        {   SaneUnit::MICROSECOND, "microseconds", "Microseconds" },
    };

    const SaneUnitDescription* find_sane_unit_description(SaneUnit unit)
    {
        for (const auto& desc : g_sane_unit_descriptions) {
            if (desc.unit == unit) {
                return &desc;
            }
        }
        return nullptr;
    }

    static_assert(static_cast<int>(SaneCap::SOFT_SELECT) == SANE_CAP_SOFT_SELECT);
    static_assert(static_cast<int>(SaneCap::HARD_SELECT) == SANE_CAP_HARD_SELECT);
    static_assert(static_cast<int>(SaneCap::SOFT_DETECT) == SANE_CAP_SOFT_DETECT);
    static_assert(static_cast<int>(SaneCap::EMULATED) == SANE_CAP_EMULATED);
    static_assert(static_cast<int>(SaneCap::AUTOMATIC) == SANE_CAP_AUTOMATIC);
    static_assert(static_cast<int>(SaneCap::INACTIVE) == SANE_CAP_INACTIVE);
    static_assert(static_cast<int>(SaneCap::ADVANCED) == SANE_CAP_ADVANCED);

    static_assert(static_cast<int>(SaneValueType::BOOL) == SANE_TYPE_BOOL);
    static_assert(static_cast<int>(SaneValueType::INT) == SANE_TYPE_INT);
    static_assert(static_cast<int>(SaneValueType::FLOAT) == SANE_TYPE_FIXED);
    static_assert(static_cast<int>(SaneValueType::STRING) == SANE_TYPE_STRING);
    static_assert(static_cast<int>(SaneValueType::BUTTON) == SANE_TYPE_BUTTON);

    static_assert(static_cast<int>(SaneOptionSetInfo::INEXACT) == SANE_INFO_INEXACT);
    static_assert(static_cast<int>(SaneOptionSetInfo::RELOAD_OPTIONS) == SANE_INFO_RELOAD_OPTIONS);
    static_assert(static_cast<int>(SaneOptionSetInfo::RELOAD_PARAMS) == SANE_INFO_RELOAD_PARAMS);

    static_assert(static_cast<int>(SaneFrameType::GRAY) == SANE_FRAME_GRAY);
    static_assert(static_cast<int>(SaneFrameType::RGB) == SANE_FRAME_RGB);
    static_assert(static_cast<int>(SaneFrameType::RED) == SANE_FRAME_RED);
    static_assert(static_cast<int>(SaneFrameType::GREEN) == SANE_FRAME_GREEN);
    static_assert(static_cast<int>(SaneFrameType::BLUE) == SANE_FRAME_BLUE);
} // namespace

std::ostream& operator<<(std::ostream& stream, const SaneDeviceInfo& data)
{
    stream << "SaneDeviceInfo{"
           << " name=" << data.name
           << " vendor=" << data.vendor
           << " model=" << data.model
           << " type=" << data.type
           << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneValueType& data)
{
    switch (data) {
        case SaneValueType::BOOL: stream << "BOOL"; break;
        case SaneValueType::INT: stream << "INT"; break;
        case SaneValueType::FLOAT: stream << "FLOAT"; break;
        case SaneValueType::STRING: stream << "STRING"; break;
        case SaneValueType::BUTTON: stream << "BUTTON"; break;
        case SaneValueType::GROUP: stream << "GROUP"; break;
        default: stream << "(unknown)"; break;
    }
    return stream;
}

std::string_view sane_unit_to_string_lower(SaneUnit unit)
{
    const auto* desc = find_sane_unit_description(unit);
    return desc ? desc->lowercase_desc : "unknown";
}

std::string_view sane_unit_to_string_upper(SaneUnit unit)
{
    const auto* desc = find_sane_unit_description(unit);
    return desc ? desc->uppercase_desc : "Unknown";
}

std::ostream& operator<<(std::ostream& stream, const SaneUnit& data)
{
    stream << sane_unit_to_string_lower(data);
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneCap& data)
{
    stream << "SaneCap(";
    if (has_flag(data, SaneCap::SOFT_SELECT)) {
        stream << " SOFT_SELECT";
    }
    if (has_flag(data, SaneCap::HARD_SELECT)) {
        stream << " HARD_SELECT";
    }
    if (has_flag(data, SaneCap::SOFT_DETECT)) {
        stream << " SOFT_DETECT";
    }
    if (has_flag(data, SaneCap::EMULATED)) {
        stream << " EMULATED";
    }
    if (has_flag(data, SaneCap::AUTOMATIC)) {
        stream << " AUTOMATIC";
    }
    if (has_flag(data, SaneCap::INACTIVE)) {
        stream << " INACTIVE";
    }
    if (has_flag(data, SaneCap::ADVANCED)) {
        stream << " ADVANCED";
    }
    stream << " )";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintNone& data)
{
    stream << "(none)";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintStringList& data)
{
    stream << "SaneConstraintStringList{";
    for (const auto& s : data.strings) {
        stream << " " << s;
    }
    stream << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintIntList& data)
{
    stream << "SaneConstraintIntList{";
    for (const auto& n : data.numbers) {
        stream << " " << n;
    }
    stream << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintFloatList& data)
{
    stream << "SaneConstraintFloatList{";
    for (const auto& n : data.numbers) {
        stream << " " << n;
    }
    stream << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintIntRange& data)
{
    stream << "SaneConstraintIntRange{"
           << " min=" << data.min
           << " max=" << data.max
           << " quantization=" << data.quantization
           << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneConstraintFloatRange& data)
{
    stream << "SaneConstraintFloatRange{"
           << " min=" << data.min
           << " max=" << data.max
           << " quantization=" << data.quantization
           << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneOptionDescriptor& data)
{
    stream << "SaneOptionDescriptor{"
           << "\n  index=" << data.index
           << "\n  name=" << data.name
           << "\n  title=" << data.title
           << "\n  description=" << data.description
           << "\n  unit=" << data.unit
           << "\n  type=" << data.type
           << "\n  size=" << data.size
           << "\n  cap=" << data.cap;
    if (auto* c = std::get_if<SaneConstraintNone>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    if (auto* c = std::get_if<SaneConstraintStringList>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    if (auto* c = std::get_if<SaneConstraintIntList>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    if (auto* c = std::get_if<SaneConstraintFloatList>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    if (auto* c = std::get_if<SaneConstraintIntRange>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    if (auto* c = std::get_if<SaneConstraintFloatRange>(&data.constraint)) {
        stream << "\n  constraint=" << *c;
    }
    stream << "\n}";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneOptionGroupDestriptor& data)
{
    stream << "SaneOptionGroupDestriptor{"
           << "\n  name=" << data.name
           << "\n  title=" << data.title
           << "\n  description=" << data.description
           << "\n  options=[";
    for (const auto& desc : data.options) {
        stream << "\n  " << desc;
    }
    stream << "\n}";
    return stream;
}

std::optional<SaneOptionDescriptor>
    find_option_descriptor(const std::vector<SaneOptionGroupDestriptor>& descriptors,
                           std::string_view name)
{
    for (const auto& group : descriptors) {
        for (const auto& option : group.options) {
            if (option.name == name) {
                return option;
            }
        }
    }
    return {};
}

bool SaneOptionValue::is_none() const
{
    return std::get_if<SaneOptionValueNone>(&value) != nullptr;
}

std::optional<bool> SaneOptionValue::as_bool() const
{
    auto* values = as_bool_vector();
    if (values == nullptr) {
        return {};
    }
    if (values->size() != 1) {
        return {};
    }
    return (*values)[0];
}

std::optional<int> SaneOptionValue::as_int() const
{
    auto* values = as_int_vector();
    if (values == nullptr) {
        return {};
    }
    if (values->size() != 1) {
        return {};
    }
    return (*values)[0];
}

std::optional<double> SaneOptionValue::as_double() const
{
    auto* values = as_double_vector();
    if (values == nullptr) {
        return {};
    }
    if (values->size() != 1) {
        return {};
    }
    return (*values)[0];
}

const std::vector<bool>* SaneOptionValue::as_bool_vector() const
{
    return std::get_if<std::vector<bool>>(&value);
}

const std::vector<int>* SaneOptionValue::as_int_vector() const
{
    return std::get_if<std::vector<int>>(&value);
}

const std::vector<double>* SaneOptionValue::as_double_vector() const
{
    return std::get_if<std::vector<double>>(&value);
}

const std::string* SaneOptionValue::as_string() const
{
    return  std::get_if<std::string>(&value);
}

std::ostream& operator<<(std::ostream& stream, const SaneOptionValue& data)
{
    stream << "SaneOptionValue{";
    if (auto* c = std::get_if<SaneOptionValueNone>(&data.value)) {
        stream << " (none)";
    } else if (auto* c = std::get_if<std::vector<bool>>(&data.value)) {
        for (auto v : *c) {
            stream << " " << v;
        }
    } else if (auto* c = std::get_if<std::vector<int>>(&data.value)) {
        for (auto v : *c) {
            stream << " " << v;
        }
    } else if (auto* c = std::get_if<std::vector<double>>(&data.value)) {
        for (auto v : *c) {
            stream << " " << v;
        }
    } else if (auto* c = std::get_if<std::string>(&data.value)) {
        stream << " " << *c;
    }
    stream << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneFrameType& data)
{
    switch (data) {
        case SaneFrameType::GRAY: stream << "GRAY"; break;
        case SaneFrameType::RGB: stream << "RGB"; break;
        case SaneFrameType::RED: stream << "RED"; break;
        case SaneFrameType::GREEN: stream << "GREEN"; break;
        case SaneFrameType::BLUE: stream << "BLUE"; break;
        default: stream << "(unknown)"; break;
    }
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneParameters& data)
{
    stream << "SaneParameters{"
           << " frame=" << data.frame
           << " last_frame=" << data.last_frame
           << " bytes_per_line=" << data.bytes_per_line
           << " pixels_per_line=" << data.pixels_per_line
           << " lines=" << data.lines
           << " depth=" << data.depth
           << " }";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SaneOptionSetInfo& data)
{
    stream << "SaneOptionSetInfo(";
    if (has_flag(data, SaneOptionSetInfo::INEXACT)) {
        stream << " INEXACT";
    }
    if (has_flag(data, SaneOptionSetInfo::RELOAD_OPTIONS)) {
        stream << " RELOAD_OPTIONS";
    }
    if (has_flag(data, SaneOptionSetInfo::RELOAD_PARAMS)) {
        stream << " RELOAD_PARAMS";
    }
    stream << " )";
    return stream;
}

} // namespace sanescan

