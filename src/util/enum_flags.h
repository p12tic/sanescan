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

#ifndef SANESCAN_UTIL_ENUM_FLAGS_H
#define SANESCAN_UTIL_ENUM_FLAGS_H

#include <type_traits>

namespace sanescan {

template<class E>
E enum_bit_or(E a, E b)
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}

template<class E>
E enum_bit_and(E a, E b)
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
}

#define SANESCAN_DECLARE_OPERATORS_FOR_SCOPED_ENUM_FLAGS(E)                                     \
    inline E operator|(E a, E b) { return ::sanescan::enum_bit_or(a, b); }                      \
    inline E operator&(E a, E b) { return ::sanescan::enum_bit_and(a, b); }                     \
    inline bool has_flag(E e, E flag) { return (e & flag) != static_cast<E>(0); }

} // namespace sanescan

#endif // SANESCAN_UTIL_ENUM_FLAGS_H
