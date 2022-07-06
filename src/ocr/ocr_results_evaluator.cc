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

#include "ocr_results_evaluator.h"
#include <algorithm>

namespace sanescan {

std::vector<OcrParagraph> evaluate_paragraphs(const std::vector<OcrParagraph>& paragraphs,
                                              double min_word_confidence)
{
    std::vector<OcrParagraph> result = paragraphs;
    for (auto& par : result) {
        // TODO: adjust boxes after cleanup
        for (auto& line : par.lines) {
            line.words.erase(std::remove_if(line.words.begin(), line.words.end(),
                                            [=](const auto& word) {
                                                return word.confidence < min_word_confidence;
                                            }),
                             line.words.end());
        }

        par.lines.erase(std::remove_if(par.lines.begin(), par.lines.end(),
                                       [](const auto& line) { return line.words.empty(); }),
                        par.lines.end());
    }
    result.erase(std::remove_if(result.begin(), result.end(),
                                [](const auto& par) { return par.lines.empty(); }),
                 result.end());
    return result;
}

} // namespace sanescan
