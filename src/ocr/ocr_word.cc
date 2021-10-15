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

#include "ocr_word.h"
#include <iostream>

namespace sanescan {

std::ostream& operator<<(std::ostream& stream, const OcrWord& word)
{
    stream << "OcrWord{\n"
           << "  char_boxes: \n";
    for (const auto& box : word.char_boxes) {
        stream << "    " << box << "\n";
    }
    stream << "  box: " << word.box << "\n"
           << "  confidence: " << word.confidence << "\n"
           << "  baseline_y: " << word.baseline_y << "\n"
           << "  baseline_angle: " << word.baseline_angle << "\n"
           << "  font_size: " << word.font_size << "\n"
           << "  content: " << word.content << "\n"
           << "}";
    return stream;
}

} // namespace sanescan
