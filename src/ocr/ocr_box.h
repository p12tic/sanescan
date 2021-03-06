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

#ifndef SANESCAN_OCR_OCR_BOX_H
#define SANESCAN_OCR_OCR_BOX_H

#include <ciso646> // needed by moc (work around https://bugreports.qt.io/browse/QTBUG-83160)
#include <compare>
#include <cstdint>
#include <iosfwd>

namespace sanescan {

struct OcrBox {
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
    std::int32_t x2 = 0;
    std::int32_t y2 = 0;

    std::int32_t width() const { return x2 - x1; }
    std::int32_t height() const { return y2 - y1; }

    auto operator<=>(const OcrBox&) const = default;
};

std::ostream& operator<<(std::ostream& stream, const OcrBox& box);

} // namespace sanescan

#endif // SANESCAN_OCR_OCR_BOX_H
