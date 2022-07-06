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

#include "ocr_pipeline_run.h"
#include "ocr_results_evaluator.h"
#include "ocr_utils.h"
#include "util/image.h"
#include "tesseract.h"

namespace sanescan {

OcrPipelineRun::OcrPipelineRun(const cv::Mat& source_image,
                               const OcrOptions& options,
                               const OcrOptions& old_options,
                               const std::optional<OcrResults>& old_results) :
    source_image_{source_image},
    options_{options},
    old_options_{old_options}
{
    mode_ = get_mode(options, old_options, old_results);
    if (mode_ == Mode::ONLY_PARAGRAPHS) {
        results_ = old_results.value();
    }
}

void OcrPipelineRun::execute()
{
    if (mode_ == Mode::FULL) {
        TesseractRecognizer recognizer{"/usr/share/tesseract-ocr/4.00/tessdata/"};
        results_.paragraphs = recognizer.recognize(source_image_);

        // Handle the case when all text within the image is rotated slightly due to the input data
        // scan just being rotated. In such case whole image will be rotated to address the following
        // issues:
        //
        // - Most PDF readers can't select rotated text properly
        // - The OCR accuracy is compromised for rotated text.
        //
        // TODO: Ideally we should detect cases when the text in the source image is legitimately
        // rotated and the rotation is not just the artifact of rotation. In such case the accuracy of
        // OCR will still be improved if rotate the source image just for OCR and then rotate the
        // results back.
        results_.adjust_angle = text_rotation_adjustment(source_image_, results_.paragraphs,
                                                         options_);
        results_.adjusted_image = source_image_;

        if (results_.adjust_angle != 0) {
            results_.adjusted_image = image_rotate_centered(results_.adjusted_image,
                                                            results_.adjust_angle);
            results_.paragraphs = recognizer.recognize(results_.adjusted_image);
        }
        results_.blur_data = compute_blur_data(results_.adjusted_image);
    }
    results_.adjusted_paragraphs = evaluate_paragraphs(results_.paragraphs,
                                                       options_.min_word_confidence);
    results_.blurred_words = detect_blur_areas(results_.blur_data, results_.adjusted_paragraphs,
                                               options_.blur_detection_coef);
}

OcrPipelineRun::Mode OcrPipelineRun::get_mode(const OcrOptions& new_options,
                                              const OcrOptions& old_options,
                                              const std::optional<OcrResults>& old_results)
{
    if (!old_results.has_value()) {
        return Mode::FULL;
    }

    auto new_options_for_full = new_options;
    new_options_for_full.min_word_confidence = 0;
    new_options_for_full.blur_detection_coef = 0;
    auto old_options_for_full = old_options;
    old_options_for_full.min_word_confidence = 0;
    old_options_for_full.blur_detection_coef = 0;

    if (new_options_for_full != old_options_for_full) {
        return Mode::FULL;
    }
    return Mode::ONLY_PARAGRAPHS;
}


} // namespace sanescan
