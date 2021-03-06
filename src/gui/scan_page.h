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

#ifndef SANESCAN_GUI_SCAN_PAGE_H
#define SANESCAN_GUI_SCAN_PAGE_H

#include "scan_type.h"
#include "ocr_job.h"
#include "lib/sane_types.h"
#include "ocr/ocr_options.h"
#include "ocr/ocr_results.h"
#include "ocr/ocr_paragraph.h"

#include <opencv2/core/mat.hpp>
#include <optional>
#include <map>

namespace sanescan {

struct PreviewConfig {
    double width_mm = 0;
    double height_mm = 0;
    unsigned dpi = 0;
};

struct ScanPage {
    ScanPage(unsigned scan_id) : scan_id{scan_id} {}
    ScanPage(ScanPage&&) = default;
    ScanPage& operator=(ScanPage&&) = default;

    // An ID that is unique across all scanned pages in a single application run.
    unsigned scan_id = 0;

    std::optional<cv::Mat> preview_image;
    PreviewConfig preview_config;
    std::optional<cv::Rect2d> preview_scan_bounds;

    std::optional<double> scan_progress;
    std::optional<cv::Mat> scanned_image;

    bool locked = false; // scanner name and options won't changed anymore
    SaneDeviceInfo device;

    // set to PREVIEW during a preview scan, reset back to NORMAL afterwards
    ScanType scan_type = ScanType::NORMAL;

    std::vector<SaneOptionGroupDestriptor> scan_option_descriptors;
    std::map<std::string, SaneOptionValue> scan_option_values;

    OcrOptions ocr_options;
    std::optional<double> ocr_progress;
    std::optional<OcrResults> ocr_results;

    std::vector<std::unique_ptr<OcrJob>> ocr_jobs;
    std::size_t last_ocr_job_id = 0;
};

} // namespace sanescan

#endif // SANESCAN_GUI_SCAN_PAGE_H
