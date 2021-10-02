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

#include "ocr_box.h"
#include <iostream>

namespace sanescan {

std::ostream& operator<<(std::ostream& stream, const OcrBox& box)
{
    stream << "OcrBox{"
           << box.x1 << ", "
           << box.y1 << ", "
           << box.x2 << ", "
           << box.y2 << "}";
    return stream;
}

} // namespace sanescan
