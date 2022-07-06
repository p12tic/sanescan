/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2022  Povilas Kanapickas <povilas@radix.lt>

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

#ifndef SANESCAN_OCR_OCR_PIPELINE_RUN_H
#define SANESCAN_OCR_OCR_PIPELINE_RUN_H

#include "ocr_options.h"
#include "ocr_results.h"
#include <optional>

namespace sanescan {

class OcrPipelineRun {
public:
    OcrPipelineRun(const cv::Mat& source_image,
                   const OcrOptions& options,
                   const OcrOptions& old_options,
                   const std::optional<OcrResults>& old_results);

    void execute();

    OcrResults& results() { return results_; }

private:

    enum class Mode {
        ONLY_PARAGRAPHS,
        FULL,
    };

    Mode get_mode(const OcrOptions& new_options, const OcrOptions& old_options,
                  const std::optional<OcrResults>& old_results);

    cv::Mat source_image_;
    OcrOptions options_;
    OcrOptions old_options_;
    Mode mode_ = Mode::FULL;

    OcrResults results_;
};

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_PIPELINE_RUN_H
