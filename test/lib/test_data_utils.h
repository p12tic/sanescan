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

#include <boost/container_hash/hash.hpp>

inline std::size_t hash_test_data(const char* data, std::size_t size)
{
    return boost::hash_range(data, data + size);
}

inline std::size_t fill_test_data(int offset, char* data, std::size_t size)
{
    char value = '0' + offset;
    for (std::size_t i = 0; i < size; ++i) {
        data[i] = value++;
    }
    return boost::hash_range(data, data + size);
}
