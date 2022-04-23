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

#ifndef SANESCAN_OCR_OCR_BASELINE_H
#define SANESCAN_OCR_OCR_BASELINE_H

#include <compare>
#include <iosfwd>

namespace sanescan {

struct OcrBaseline {
    // Baseline is calculated by taking bottom left corner of the bounding box as origin (X0, Y0)
    // and drawing a line Y = Y0 + baseline_y + (X - (X0 + baseline_x)) * std::tan(angle).
    // The angle is in clockwise direction from the horizontal +X axis
    double x = 0;
    double y = 0;
    double angle = 0;

    auto operator<=>(const OcrBaseline&) const = default;
};

std::ostream& operator<<(std::ostream& stream, const OcrBaseline& baseline);

} // namespace sanescan

#endif
