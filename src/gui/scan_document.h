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

#ifndef SANESCAN_GUI_SCAN_DOCUMENT_H
#define SANESCAN_GUI_SCAN_DOCUMENT_H

#include "lib/sane_types.h"
#include <opencv2/core/mat.hpp>
#include <optional>
#include <map>

namespace sanescan {

struct ScanDocument {
    // An ID that is unique across all scanned documents in a single application run.
    unsigned scan_id = 0;

    cv::Mat source_image;

    std::string scanner_name;
    std::vector<SaneOptionGroupDestriptor> scan_option_descriptors;
    std::map<std::string, SaneOptionValue> scan_option_values;
};

} // namespace sanescan

#endif // SANESCAN_GUI_SCAN_DOCUMENT_H
