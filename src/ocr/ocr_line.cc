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

#include "ocr_line.h"
#include <iostream>

namespace sanescan {

std::ostream& operator<<(std::ostream& stream, const OcrLine& line)
{
    stream << "OcrLine{\n"
           << "  words: \n";
    for (const auto& word : line.words) {
        stream << "    " << word << "\n";
    }
    stream << "  box: " << line.box << "\n"
           << "  baseline_y: " << line.baseline_y << "\n"
           << "  baseline_coeff: " << line.baseline_coeff << "\n"
           << "}";
    return stream;
}

} // namespace sanescan
