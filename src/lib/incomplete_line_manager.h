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

#ifndef SANESCAN_LIB_INCOMPLETE_LINE_MANAGER_H
#define SANESCAN_LIB_INCOMPLETE_LINE_MANAGER_H

#include <cstring>
#include <tuple>
#include <vector>

namespace sanescan {

class IncompleteLineManager {
public:
    // returns updated buffer and buffer size
    std::tuple<char*, std::size_t> before_read(char* buffer, std::size_t buffer_size)
    {
        partial_bytes_count_ = partial_line_.size();
        if (partial_bytes_count_) {
            std::memcpy(buffer, partial_line_.data(), partial_bytes_count_);
            buffer += partial_bytes_count_;
            partial_line_.clear();
        }
        return {buffer, buffer_size - partial_bytes_count_};
    }

    // returns updated bytes_written
    std::size_t after_read(char* buffer, std::size_t bytes_written, std::size_t bytes_per_line)
    {
        auto total_bytes_written = bytes_written + partial_bytes_count_;
        partial_bytes_count_ = 0;

        auto incomplete_bytes = total_bytes_written % bytes_per_line;
        if (incomplete_bytes != 0) {
            partial_line_.assign(buffer + bytes_written - incomplete_bytes,
                                 buffer + bytes_written);
            total_bytes_written -= incomplete_bytes;
        }

        return total_bytes_written;
    }

private:
    std::size_t partial_bytes_count_ = 0;
    std::vector<char> partial_line_;
};


} // namespace sanescan

#endif // SANESCAN_LIB_INCOMPLETE_LINE_MANAGER_H
