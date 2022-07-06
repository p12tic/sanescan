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

#ifndef SANESCAN_GUI_OCR_JOB_H
#define SANESCAN_GUI_OCR_JOB_H

#include "lib/job_queue.h"
#include "ocr/ocr_pipeline_run.h"

#include <opencv2/core/mat.hpp>
#include <atomic>
#include <optional>

namespace sanescan {

// Note that we must
struct OcrJob : IJob {
public:
    OcrJob(const cv::Mat& source_image, const OcrOptions& options,
           const OcrOptions& old_options, const std::optional<OcrResults>& old_results,
           std::size_t job_id, std::function<void()> on_finish);

    ~OcrJob() override;
    void execute() override;
    void cancel() override;

    OcrResults& results() { return run_.results(); }
    std::size_t job_id() const { return job_id_; }
    bool finished() const { return finished_; }

private:
    cv::Mat source_image_storage_;

    // cv::Mat contains an internal ref-counter. Thus simply doing cv::Mat x = run.source_image_; in
    // worker thread context would introduce data race. We work around this by storing a an
    // instance of the original cv::Mat within IJob as source_image_storage_. This ensures the data
    // stays valid for the duration of the job. Then we assign the data as "external data" to
    // run.source_image_. This would force opencv to not do any ref-counting operations on data that
    // is used outside worker thread context.

    OcrPipelineRun run_;
    std::size_t job_id_ = 0;
    std::atomic<bool> finished_;
    std::function<void()> on_finish_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_OCR_JOB_H
