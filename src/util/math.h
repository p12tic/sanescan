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

#ifndef SANESCAN_UTIL_MATH_H
#define SANESCAN_UTIL_MATH_H

#include <boost/math/constants/constants.hpp>
#include <cmath>

namespace sanescan {

// Like std::fmod, but the remainder falls into range of [-y/2, y/2]
inline double near_zero_fmod(double x, double y)
{
    x = std::fmod(x, y);
    if (x < - y / 2) {
        x += y;
    }
    if (x > y / 2) {
        x -= y;
    }
    return x;
}

inline double positive_fmod(double x, double y)
{
    x = std::fmod(x, y);
    if (x < 0) {
        x += y;
    }
    return x;
}

constexpr double deg_to_rad(double deg)
{
    return deg * boost::math::double_constants::pi / 180.0;
}

constexpr double rad_to_deg(double rad)
{
    return rad * 180.0 / boost::math::double_constants::pi;
}

constexpr double inch_to_mm(double inch)
{
    return inch * 25.4;
}

constexpr double mm_to_inch(double inch)
{
    return inch / 25.4;
}

} // namespace sanescan

#endif // SANESCAN_UTIL_MATH_H
