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

#include "test_data_utils.h"
#include "lib/incomplete_line_manager.h"
#include <gtest/gtest.h>

TEST(IncompleteLineManager, NoUpdatesOnFullLines)
{
    sanescan::IncompleteLineManager manager;

    std::string orig_data = "01234567890123456789";
    std::string data = orig_data;
    auto [buffer, size] = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data());
    ASSERT_EQ(size, data.size());

    auto written_bytes = manager.after_read(buffer, 12, 3);
    ASSERT_EQ(written_bytes, 12);

    std::tie(buffer, size) = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data());
    ASSERT_EQ(size, data.size());
    ASSERT_EQ(data, orig_data);
}

TEST(IncompleteLineManager, UpdatesOnPartialLines)
{
    sanescan::IncompleteLineManager manager;

    std::string no_data = "xxxxxxxxxxxxxxxx";
    std::string written_data = "01234567890123456789";
    std::string data = no_data;

    auto [buffer, size] = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data());
    ASSERT_EQ(size, data.size());

    auto written_bytes = manager.after_read(written_data.data(), 12, 5);
    ASSERT_EQ(written_bytes, 10);

    std::tie(buffer, size) = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data() + 2);
    ASSERT_EQ(size, data.size() - 2);
    ASSERT_EQ(data, "01xxxxxxxxxxxxxx");

    written_bytes = manager.after_read(written_data.data(), 13, 5);
    ASSERT_EQ(written_bytes, 15);
}

TEST(IncompleteLineManager, UpdatesOnPartialLinesMultiple)
{
    sanescan::IncompleteLineManager manager;

    std::string no_data = "xxxxxxxxxxxxxxxx";
    std::string written_data = "01234567890123456789";
    std::string data = no_data;

    auto [buffer, size] = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data());
    ASSERT_EQ(size, data.size());

    auto written_bytes = manager.after_read(written_data.data(), 12, 5);
    ASSERT_EQ(written_bytes, 10);

    std::tie(buffer, size) = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data() + 2);
    ASSERT_EQ(size, data.size() - 2);
    ASSERT_EQ(data, "01xxxxxxxxxxxxxx");
    data = no_data;

    written_bytes = manager.after_read(written_data.data(), 11, 5);
    ASSERT_EQ(written_bytes, 10);

    std::tie(buffer, size) = manager.before_read(data.data(), data.size());
    ASSERT_EQ(buffer, data.data() + 3);
    ASSERT_EQ(size, data.size() - 3);
    ASSERT_EQ(data, "890xxxxxxxxxxxxx");
    data = no_data;

    written_bytes = manager.after_read(written_data.data(), 12, 5);
    ASSERT_EQ(written_bytes, 15);
}
