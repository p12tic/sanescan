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

#ifndef SANESCAN_LIB_SANE_TYPES_H
#define SANESCAN_LIB_SANE_TYPES_H

#include "util/enum_flags.h"
#include <ciso646> // needed by moc (work around https://bugreports.qt.io/browse/QTBUG-83160)
#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <vector>

namespace sanescan {

/// Corresponds to SANE_Device type
struct SaneDeviceInfo {
    std::string name;
    std::string vendor;
    std::string model;
    std::string type;
};

std::ostream& operator<<(std::ostream& stream, const SaneDeviceInfo& data);

enum class SaneValueType {
    BOOL = 0,
    INT,
    FLOAT,
    STRING,
    BUTTON,
    GROUP,
};

std::ostream& operator<<(std::ostream& stream, const SaneValueType& data);

enum class SaneUnit {
    NONE,
    PIXEL,
    BIT,
    MM,
    DPI,
    PERCENT,
    MICROSECOND
};

std::string_view sane_unit_to_string_lower(SaneUnit unit);
std::string_view sane_unit_to_string_upper(SaneUnit unit);

std::ostream& operator<<(std::ostream& stream, const SaneUnit& data);

enum class SaneCap {
    NONE = 0,
    SOFT_SELECT = 1 << 0,
    HARD_SELECT = 1 << 1,
    SOFT_DETECT = 1 << 2,
    EMULATED = 1 << 3,
    AUTOMATIC = 1 << 4,
    INACTIVE = 1 << 5,
    ADVANCED = 1 << 6,
};

SANESCAN_DECLARE_OPERATORS_FOR_SCOPED_ENUM_FLAGS(SaneCap)

std::ostream& operator<<(std::ostream& stream, const SaneCap& data);

struct SaneConstraintNone {
    std::strong_ordering operator<=>(const SaneConstraintNone& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintNone& data);

struct SaneConstraintStringList {
    std::vector<std::string> strings;

    std::strong_ordering operator<=>(const SaneConstraintStringList& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintStringList& data);

struct SaneConstraintIntList {
    std::vector<int> numbers;

    std::strong_ordering operator<=>(const SaneConstraintIntList& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintIntList& data);

struct SaneConstraintFloatList {
    std::vector<double> numbers;

    std::partial_ordering operator<=>(const SaneConstraintFloatList& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintFloatList& data);

/// corresponds to SANE_Range type when option is INT
struct SaneConstraintIntRange {
    int min = 0;
    int max = 0;
    int quantization = 0;

    std::strong_ordering operator<=>(const SaneConstraintIntRange& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintIntRange& data);

/// corresponds to SANE_Range type when option is FLOAT
struct SaneConstraintFloatRange {
    double min = 0;
    double max = 0;
    double quantization = 0;

    std::partial_ordering operator<=>(const SaneConstraintFloatRange& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneConstraintFloatRange& data);

/// corresponds to SANE_Option_Descriptor
struct SaneOptionDescriptor {
    std::size_t index = 0; // index of the option to be sent to set_option or get_option
    std::string name;
    std::string title;
    std::string description;
    SaneUnit unit = SaneUnit::NONE;
    SaneValueType type = SaneValueType::GROUP;

    // differently from SANE_Option_Descriptor, in cases of bool, integer or float values
    // this member contains the number of values, not the size of the data.
    int size = 0;
    SaneCap cap = SaneCap::NONE;

    std::variant<
        SaneConstraintNone,
        SaneConstraintStringList, // only if type == SaneValueType::STRING
        SaneConstraintIntList, // only if type == SaneValueType::INT
        SaneConstraintFloatList, // only if type == SaneValueType::FLOAT
        SaneConstraintIntRange, // only if type == SaneValueType::INT
        SaneConstraintFloatRange // only if type == SaneValueType::FLOAT
    > constraint;

    std::partial_ordering operator<=>(const SaneOptionDescriptor& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneOptionDescriptor& data);

struct SaneOptionGroupDestriptor {
    std::string name;
    std::string title;
    std::string description;
    std::vector<SaneOptionDescriptor> options;

    std::partial_ordering operator<=>(const SaneOptionGroupDestriptor& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneOptionGroupDestriptor& data);

std::optional<SaneOptionDescriptor>
    find_option_descriptor(const std::vector<SaneOptionGroupDestriptor>& descriptors,
                           std::string_view name);

struct SaneOptionValueNone {
    std::strong_ordering operator<=>(const SaneOptionValueNone& other) const = default;
};

using SaneOptionValueVariant = std::variant<
    SaneOptionValueNone,
    std::vector<bool>, // for both bool and button
    std::vector<int>,
    std::vector<double>,
    std::string
>;

struct SaneOptionValue {
    SaneOptionValueVariant value;

    SaneOptionValue(SaneOptionValueNone value) : value{value} {}
    SaneOptionValue(bool value) : value{std::vector<bool>{value}} {}
    SaneOptionValue(int value) : value{std::vector<int>{value}} {}
    SaneOptionValue(double value) : value{std::vector<double>{value}} {}
    SaneOptionValue(std::vector<bool> value) : value{std::move(value)} {}
    SaneOptionValue(std::vector<int> value) : value{std::move(value)} {}
    SaneOptionValue(std::vector<double> value) : value{std::move(value)} {}
    SaneOptionValue(std::string value) : value{std::move(value)} {}

    bool is_none() const;
    std::optional<bool> as_bool() const;
    std::optional<int> as_int() const;
    std::optional<double> as_double() const;

    const std::vector<bool>* as_bool_vector() const;
    const std::vector<int>* as_int_vector() const;
    const std::vector<double>* as_double_vector() const;

    const std::string* as_string() const;

    std::partial_ordering operator<=>(const SaneOptionValue& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneOptionValue& data);

struct SaneOptionIndexedValue {
    SaneOptionIndexedValue(std::size_t index, const SaneOptionValue& value) :
        index{index}, value{value}
    {}

    std::size_t index = 0;
    SaneOptionValue value;

    std::strong_ordering operator<=>(const SaneOptionIndexedValue& other) const = default;
};

/// Corresponds to SANE_Frame
enum class SaneFrameType {
    GRAY = 0,
    RGB,
    RED,
    GREEN,
    BLUE
};

std::ostream& operator<<(std::ostream& stream, const SaneFrameType& data);

struct SaneParameters {
    SaneFrameType frame = SaneFrameType::GRAY;
    bool last_frame = false;
    int bytes_per_line = 0;
    int pixels_per_line = 0;
    int lines = 0;
    int depth = 0;

    std::strong_ordering operator<=>(const SaneParameters& other) const = default;
};

std::ostream& operator<<(std::ostream& stream, const SaneParameters& data);

/// Corresponts to SANE_INFO_*
enum class SaneOptionSetInfo {
    NONE,
    INEXACT = 1 << 0,
    RELOAD_OPTIONS = 1 << 1,
    RELOAD_PARAMS = 1 << 2,
};

SANESCAN_DECLARE_OPERATORS_FOR_SCOPED_ENUM_FLAGS(SaneOptionSetInfo)

std::ostream& operator<<(std::ostream& stream, const SaneOptionSetInfo& data);

} // namespace sanescan

#endif
