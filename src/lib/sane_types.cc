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

bool SaneConstraintStringList::operator==(const SaneConstraintStringList& other) const
{
    return strings == other.strings;
}

bool SaneConstraintIntList::operator==(const SaneConstraintIntList& other) const
{
    return numbers == other.numbers;
}

bool SaneConstraintFloatList::operator==(const SaneConstraintFloatList& other) const
{
    return numbers == other.numbers;
}

bool SaneConstraintRange::operator==(const SaneConstraintRange& other) const
{
    return min == other.min && max == other.max && quantization == other.quantization;
}

bool SaneOptionDescriptor::operator==(const SaneOptionDescriptor& other) const
{
    return index == other.index &&
            name == other.name &&
            title == other.title &&
            description == other.description &&
            unit == other.unit &&
            type == other.type &&
            size == other.size &&
            cap == other.cap &&
            constraint == other.constraint;
}

bool SaneOptionGroupDestriptor::operator==(const SaneOptionGroupDestriptor& other) const
{
    return name == other.name &&
            title == other.title &&
            description == other.description &&
            options == other.options;
}

} // namespace sanescan

