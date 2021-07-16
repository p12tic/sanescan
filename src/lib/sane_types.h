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

#include <string>
#include <string_view>
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

enum class SaneValueType {
    BOOL = 0,
    INT,
    FLOAT,
    STRING,
    BUTTON,
    GROUP,
};

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


enum class SaneCap {
    SOFT_SELECT = 1 << 0,
    HARD_SELECT = 1 << 1,
    SOFT_DETECT = 1 << 2,
    EMULATED = 1 << 3,
    AUTOMATIC = 1 << 4,
    INACTIVE = 1 << 5,
    ADVANCED = 1 << 6,
};

inline SaneCap operator|(SaneCap lhs, SaneCap rhs)
{
    return static_cast<SaneCap>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline SaneCap operator&(SaneCap lhs, SaneCap rhs)
{
    return static_cast<SaneCap>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

struct SaneConstraintNone {};

struct SaneConstraintStringList {
    std::vector<std::string> strings;
};

struct SaneConstraintNumberList {
    std::vector<int> numbers;
};

/// corresponds to SANE_Range type
struct SaneConstraintRange {
    int min = 0;
    int max = 0;
    int quantization = 0;
};

/// corresponds to SANE_Option_Descriptor
struct SaneOptionDescriptor {
    std::size_t index; // index of the option to be sent to set_option or get_option
    std::string name;
    std::string title;
    std::string description;
    SaneUnit unit;
    SaneValueType type;
    int size;
    SaneCap cap;

    std::variant<
        SaneConstraintNone,
        SaneConstraintStringList,
        SaneConstraintNumberList,
        SaneConstraintRange
    > constraint;
};

struct SaneOptionGroupDestriptor {
    std::string name;
    std::string title;
    std::string description;
    std::vector<SaneOptionDescriptor> options;
};

using SaneOptionValue = std::variant<
    std::vector<bool>, // for both bool and button
    std::vector<int>,
    std::vector<double>,
    std::string
>;

/// Corresponds to SANE_Frame
enum class SaneFrameType {
    GRAY = 0,
    RGB,
    RED,
    GREEN,
    BLUE
};

struct SaneParameters {
    SaneFrameType frame = SaneFrameType::GRAY;
    bool last_frame = false;
    int bytes_per_line = 0;
    int pixels_per_line = 0;
    int lines = 0;
    int depth = 0;
};

/// Corresponts to SANE_INFO_*
enum class SaneOptionSetInfo {
    NONE,
    INEXACT = 1 << 0,
    RELOAD_OPTIONS = 1 << 1,
    RELOAD_PARAMS = 1 << 2,
};

inline SaneOptionSetInfo operator|(SaneOptionSetInfo lhs, SaneOptionSetInfo rhs)
{
    return static_cast<SaneOptionSetInfo>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline SaneOptionSetInfo operator&(SaneOptionSetInfo lhs, SaneOptionSetInfo rhs)
{
    return static_cast<SaneOptionSetInfo>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

} // namespace sanescan

#endif