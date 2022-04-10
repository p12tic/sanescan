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

#include "ocr_job.h"
#include "ocr/tesseract.h"

namespace sanescan {

OcrJob::OcrJob(const cv::Mat& source_image, const OcrOptions& options, std::size_t job_id,
               std::function<void()> on_finish) :
    source_image_storage_{source_image},
    options_{options},
    job_id_{job_id},
    on_finish_{on_finish}
{
    source_image_ = cv::Mat(source_image_storage_.size.dims(),
                            source_image_storage_.size.p,
                            source_image_storage_.type(),
                            source_image_storage_.data,
                            source_image_storage_.step.p);
}

OcrJob::~OcrJob() = default;

void OcrJob::execute()
{
    TesseractRecognizer recognizer{"/usr/share/tesseract-ocr/4.00/tessdata/"};
    results_ = recognizer.recognize(source_image_, options_);
    finished_ = true;
    on_finish_();
}

void OcrJob::cancel()
{
}

} // namespace sanescan
