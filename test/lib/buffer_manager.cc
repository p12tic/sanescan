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

#include "lib/buffer_manager.h"
#include <boost/container_hash/hash.hpp>
#include <gtest/gtest.h>

namespace {

std::size_t hash_test_data(const char* data, std::size_t size)
{
    return boost::hash_range(data, data + size);
}

std::size_t fill_test_data(int offset, char* data, std::size_t size)
{
    char value = '0' + offset;
    for (std::size_t i = 0; i < size; ++i) {
        data[i] = value++;
    }
    return boost::hash_range(data, data + size);
}


} // namespace

TEST(BufferManager, SingleWriteSingleReadLoop)
{
    auto manager = sanescan::BufferManager(120);

    for (int i = 0; i < 10; ++i) {
        auto first_line = i * 2 + 1;
        auto last_line = i * 2 + 3;

        ASSERT_FALSE(manager.get_read().has_value());

        auto buf1_write = manager.get_write(first_line, last_line, 20);
        ASSERT_TRUE(buf1_write.has_value());
        ASSERT_EQ(buf1_write->size(), 40);
        auto hash_written1 = fill_test_data(i, buf1_write->data(), buf1_write->size());

        ASSERT_FALSE(manager.get_read().has_value());

        buf1_write->finish(40);

        auto buf1_read = manager.get_read();
        ASSERT_TRUE(buf1_read.has_value());
        ASSERT_EQ(buf1_read->first_line(), first_line);
        ASSERT_EQ(buf1_read->last_line(), last_line);
        ASSERT_EQ(buf1_read->line_bytes(), 20);
        ASSERT_EQ(hash_written1, hash_test_data(buf1_read->data(), buf1_read->size()));
        buf1_read->finish();
    }
}

TEST(BufferManager, TwoWritesTwoFinishTwoReadsLoop)
{
    auto manager = sanescan::BufferManager(120);

    for (int i = 0; i < 20; i += 2) {
        auto first_line1 = i * 2 + 1;
        auto last_line1 = i * 2 + 3;
        auto first_line2 = i * 2 + 5;
        auto last_line2 = i * 2 + 7;

        ASSERT_FALSE(manager.get_read().has_value());

        auto buf1_write = manager.get_write(first_line1, last_line1, 20);
        ASSERT_TRUE(buf1_write.has_value());
        ASSERT_EQ(buf1_write->size(), 40);
        auto hash_written1 = fill_test_data(i, buf1_write->data(), buf1_write->size());

        auto buf2_write = manager.get_write(first_line2, last_line2, 20);
        ASSERT_TRUE(buf2_write.has_value());
        ASSERT_EQ(buf2_write->size(), 40);
        auto hash_written2 = fill_test_data(i + 1, buf2_write->data(), buf2_write->size());

        ASSERT_FALSE(manager.get_read().has_value());

        buf1_write->finish(40);
        buf2_write->finish(40);

        auto buf1_read = manager.get_read();
        ASSERT_TRUE(buf1_read.has_value());
        ASSERT_EQ(buf1_read->first_line(), first_line1);
        ASSERT_EQ(buf1_read->last_line(), last_line1);
        ASSERT_EQ(buf1_read->line_bytes(), 20);
        ASSERT_EQ(hash_written1, hash_test_data(buf1_read->data(), buf1_read->size()));
        buf1_read->finish();

        auto buf2_read = manager.get_read();
        ASSERT_TRUE(buf2_read.has_value());
        ASSERT_EQ(buf2_read->first_line(), first_line2);
        ASSERT_EQ(buf2_read->last_line(), last_line2);
        ASSERT_EQ(buf2_read->line_bytes(), 20);
        ASSERT_EQ(hash_written2, hash_test_data(buf2_read->data(), buf2_read->size()));
        buf2_read->finish();
    }
}

TEST(BufferManager, UnavailableWriteNotFinished)
{
    auto manager = sanescan::BufferManager(120);
    ASSERT_TRUE(manager.get_write(1, 3, 20).has_value());
    ASSERT_TRUE(manager.get_write(3, 5, 20).has_value());
    ASSERT_TRUE(manager.get_write(5, 7, 20).has_value());
    ASSERT_FALSE(manager.get_write(7, 9, 20).has_value());
}

TEST(BufferManager, UnavailableWriteWhenFinished)
{
    auto manager = sanescan::BufferManager(120);
    auto buf = manager.get_write(1, 3, 20);
    ASSERT_TRUE(buf.has_value());
    buf->finish(40);
    buf = manager.get_write(3, 5, 20);
    ASSERT_TRUE(buf.has_value());
    buf->finish(40);
    buf = manager.get_write(5, 7, 20);
    ASSERT_TRUE(buf.has_value());
    buf->finish(40);
    ASSERT_FALSE(manager.get_write(7, 9, 20).has_value());
}

TEST(BufferManager, ResetClearsUnfinishedWrites)
{
    auto manager = sanescan::BufferManager(120);
    {
        ASSERT_TRUE(manager.get_write(1, 3, 20).has_value());
        ASSERT_TRUE(manager.get_write(3, 5, 20).has_value());
    }
    manager.reset();
    {
        ASSERT_TRUE(manager.get_write(5, 7, 20).has_value());
        ASSERT_TRUE(manager.get_write(7, 9, 20).has_value());
        ASSERT_TRUE(manager.get_write(9, 11, 20).has_value());
        ASSERT_FALSE(manager.get_write(11, 13, 20).has_value());
    }
}
