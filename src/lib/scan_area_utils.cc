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

#include "scan_area_utils.h"

namespace sanescan {

namespace {

cv::Rect2d normalized(const cv::Rect2d& rect)
{
    if (rect.width > 0 && rect.height > 0) {
        return rect;
    }

    auto x = rect.x;
    auto y = rect.y;
    auto w = rect.width;
    auto h = rect.height;
    if (w < 0) {
        x -= w;
        w = -w;
    }
    if (h < 0) {
        y -= h;
        h = -h;
    }
    return cv::Rect2d(x, y, w, h);
}

} // namespace

std::optional<cv::Rect2d>
    get_curr_scan_area_from_options(const std::map<std::string, SaneOptionValue>& options)
{
    auto get_value = [&](const std::string& name) -> std::optional<double>
    {
        if (options.count(name) == 0) {
            return {};
        }
        return options.at(name).as_double();
    };

    auto value_tl_x = get_value("tl-x");
    auto value_tl_y = get_value("tl-y");
    auto value_br_x = get_value("br-x");
    auto value_br_y = get_value("br-y");

    if (!value_tl_x.has_value() || !value_tl_y.has_value() ||
        !value_br_x.has_value() || !value_br_y.has_value()) {
        return {};
    }

    return cv::Rect2d{value_tl_x.value(), value_tl_y.value(),
                      value_br_x.value() - value_tl_x.value(),
                      value_br_y.value() - value_tl_y.value()};
}

std::optional<cv::Rect2d>
    get_scan_size_from_options(const std::vector<SaneOptionGroupDestriptor>& options)
{
    auto tl_x_desc = find_option_descriptor(options, "tl-x");
    auto tl_y_desc = find_option_descriptor(options, "tl-y");
    auto br_x_desc = find_option_descriptor(options, "br-x");
    auto br_y_desc = find_option_descriptor(options, "br-y");

    if (!tl_x_desc || !tl_y_desc || !br_x_desc || !br_y_desc) {
        return {};
    }

    auto* tl_x_constraint = std::get_if<SaneConstraintFloatRange>(&tl_x_desc->constraint);
    auto* tl_y_constraint = std::get_if<SaneConstraintFloatRange>(&tl_y_desc->constraint);
    auto* br_x_constraint = std::get_if<SaneConstraintFloatRange>(&br_x_desc->constraint);
    auto* br_y_constraint = std::get_if<SaneConstraintFloatRange>(&br_y_desc->constraint);

    if (!tl_x_constraint || !tl_y_constraint || !br_x_constraint || !br_y_constraint) {
        return {};
    }

    cv::Rect2d rect = {tl_x_constraint->min, tl_y_constraint->min,
                       br_x_constraint->max - tl_x_constraint->min,
                       br_y_constraint->max - tl_y_constraint->min};
    return normalized(rect);
}

std::optional<SaneOptionValue>
    get_min_resolution(const std::vector<SaneOptionGroupDestriptor>& option_groups)
{
    auto resolution = find_option_descriptor(option_groups, "resolution");
    if (!resolution.has_value()) {
        return {};
    }

    if (auto* constraint = std::get_if<SaneConstraintFloatList>(&resolution.value().constraint)) {
        if (constraint->numbers.empty()) {
            return {};
        }
        auto min_value = *std::min_element(constraint->numbers.begin(), constraint->numbers.end());
        return SaneOptionValue{min_value};
    }
    if (auto* constraint = std::get_if<SaneConstraintIntList>(&resolution.value().constraint)) {
        if (constraint->numbers.empty()) {
            return {};
        }
        auto min_value = *std::min_element(constraint->numbers.begin(), constraint->numbers.end());
        return SaneOptionValue{min_value};
    }
    if (auto* constraint = std::get_if<SaneConstraintFloatRange>(&resolution.value().constraint)) {
        return SaneOptionValue{constraint->min};
    }
    if (auto* constraint = std::get_if<SaneConstraintIntRange>(&resolution.value().constraint)) {
        return SaneOptionValue{constraint->min};
    }
    return {};
}

} // namespace sanescan
