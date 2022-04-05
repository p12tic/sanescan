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

#ifndef SANESCAN_LIB_SCAN_AREA_UTILS_H
#define SANESCAN_LIB_SCAN_AREA_UTILS_H

#include "sane_types.h"
#include <opencv2/core/types.hpp>
#include <map>
#include <optional>
#include <string>

namespace sanescan {

std::optional<cv::Rect2d>
    get_curr_scan_area_from_options(const std::map<std::string, SaneOptionValue>& options);

std::optional<cv::Rect2d>
    get_scan_size_from_options(const std::vector<SaneOptionGroupDestriptor>& options);

} // namespace sanescan

#endif // SANESCAN_LIB_SCAN_AREA_UTILS_H
