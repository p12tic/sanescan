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

#include "sane_types_conv.h"
#include "sane_exception.h"

namespace sanescan {

namespace {

int convert_sane_option_size(SANE_Value_Type type, int size)
{
    switch (type) {
        case SANE_TYPE_BOOL:
        case SANE_TYPE_INT:
        case SANE_TYPE_FIXED:
            return size / sizeof(SANE_Word);
        default:
            return size;
    }
}

} // namespace

SaneOptionDescriptor sane_option_descriptor_to_sanescan(int index,
                                                        const SANE_Option_Descriptor* desc)
{
    SaneOptionDescriptor option;
    option.index = index;
    option.name = desc->name;
    option.title = desc->title;
    option.description = desc->desc;
    option.unit = sane_unit_to_sanescan(desc->unit);
    option.type = sane_value_type_to_sanescan(desc->type);
    option.size = convert_sane_option_size(desc->type, desc->size);
    option.cap = sane_cap_to_sanescan(desc->cap);

    switch (desc->constraint_type) {
        default:
        case SANE_CONSTRAINT_NONE: {
            option.constraint = SaneConstraintNone{};
            break;
        }
        case SANE_CONSTRAINT_RANGE: {
            const auto* range = desc->constraint.range;
            switch (option.type) {
                case SaneValueType::INT: {
                    option.constraint = SaneConstraintIntRange{range->min, range->max, range->quant};
                    break;
                }
                case SaneValueType::FLOAT: {
                    option.constraint = SaneConstraintFloatRange{SANE_UNFIX(range->min),
                                                                 SANE_UNFIX(range->max),
                                                                 SANE_UNFIX(range->quant)};
                    break;
                }
                default:
                    throw SaneException("word list constraint used on wrong option type " +
                                        std::to_string(desc->type));
            }
            break;
        }
        case SANE_CONSTRAINT_STRING_LIST: {
            const SANE_String_Const* ptr = desc->constraint.string_list;
            SaneConstraintStringList constraint;
            while (*ptr != nullptr) {
                constraint.strings.push_back(*ptr++);
            }
            option.constraint = std::move(constraint);
            break;
        }
        case SANE_CONSTRAINT_WORD_LIST: {
            const SANE_Word* ptr = desc->constraint.word_list;
            switch (option.type) {
                case SaneValueType::INT: {
                    SaneConstraintIntList constraint;
                    int length = *ptr++;
                    constraint.numbers.assign(ptr, ptr + length);
                    option.constraint = std::move(constraint);
                    break;
                }
                case SaneValueType::FLOAT: {
                    SaneConstraintFloatList constraint;
                    int length = *ptr++;
                    constraint.numbers.reserve(length);
                    for (int i = 0; i < length; ++i) {
                        constraint.numbers.push_back(SANE_UNFIX(*ptr++));
                    }
                    option.constraint = std::move(constraint);
                    break;
                }
                default:
                    throw SaneException("word list constraint used on wrong option type " +
                                        std::to_string(desc->type));
            }
            break;
        }
    }
    return option;
}

SaneOptionGroupDestriptor
    sane_option_descriptor_to_sanescan_group(const SANE_Option_Descriptor* desc)
{
    SaneOptionGroupDestriptor option_group;
    option_group.name = desc->name;
    option_group.title = desc->title;
    option_group.description = desc->desc;
    return option_group;
}

SaneParameters sane_parameters_to_sanescan(const SANE_Parameters& params)
{
    SaneParameters result;
    result.frame = sane_frame_type_to_sanescan(params.format);
    result.last_frame = params.last_frame;
    result.bytes_per_line = params.bytes_per_line;
    result.pixels_per_line = params.pixels_per_line;
    result.lines = params.lines;
    result.depth = params.depth;
    return result;
}

} // namespace sanescan
