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

} // namespace sanescan

#endif // SANESCAN_UTIL_MATH_H
