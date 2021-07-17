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

#ifndef SANESCAN_LIB_BUFFER_MANAGER_H
#define SANESCAN_LIB_BUFFER_MANAGER_H

#include <cstddef>
#include <memory>
#include <optional>

namespace sanescan {

class BufferManager;

class BufferReadRef {
public:
    BufferReadRef(BufferReadRef&& other);
    BufferReadRef& operator=(BufferReadRef&& other);

    ~BufferReadRef(); // calls finish()

    const char* data() const { return data_; }
    std::size_t size() const { return (last_line_ - first_line_) * line_bytes_; }
    std::size_t first_line() const { return first_line_; }
    std::size_t last_line() const { return last_line_; }
    std::size_t line_bytes() const { return line_bytes_; }

    /** Finishes the read. This can be called multiple times, only the first has any effect.
        After the first call, the data pointed to by data() must not be accessed.
    */
    void finish();

private:
    friend class BufferManager;

    BufferReadRef(BufferManager* manager, std::size_t index, const char* data,
                  std::size_t first_line, std::size_t last_line, std::size_t line_bytes) :
        manager_{manager},
        index_{index},
        data_{data},
        first_line_{first_line},
        last_line_{last_line},
        line_bytes_{line_bytes}
    {}

    // non-copyable because we need to accurately track finished status
    BufferReadRef(const BufferReadRef& other) = delete;
    BufferReadRef& operator=(const BufferReadRef&) = delete;

    BufferManager* manager_ = nullptr;
    std::size_t index_ = 0;
    const char* data_ = nullptr;
    std::size_t first_line_ = 0;
    std::size_t last_line_ = 0;
    std::size_t line_bytes_ = 0;
    bool finished_ = false;
};

class BufferWriteRef {
public:
    char* data() { return data_; }
    std::size_t size() { return size_; }

    BufferWriteRef(BufferWriteRef&& other);
    BufferWriteRef& operator=(BufferWriteRef&& other);

    // calls finish(0) if it wasn't called before
    ~BufferWriteRef();

    /** Finish the write noting the actually read number of bytes. If the written size is less than
        the size of the buffer, any partially written lines are discarded. This can be called
        multiple times, only the first has any effect.

        After the first call, the data pointed to by data() must not be accessed.
    */
    void finish(std::size_t size);

private:
    friend class BufferManager;

    BufferWriteRef(BufferManager* manager, std::size_t index, char* data, std::size_t size) :
        manager_{manager},
        index_{index},
        data_{data},
        size_{size}
    {}
    // non-copyable because we need to accurately track finished status
    BufferWriteRef(const BufferWriteRef&) = delete;
    BufferWriteRef& operator=(const BufferWriteRef&) = delete;

    BufferManager* manager_ = nullptr;
    std::size_t index_ = 0;
    char* data_ = nullptr;
    std::size_t size_ = 0;
    bool finished_ = false;
};

/** A simple buffer manager to buffer data during scanning for communication between UI and
    scanning threads.

    There are perhaps thousands of buffering implentations available, they were not reused due to
    specific requirements in this case:
     - This is not a simple circular buffer because the data flow is subdivided into sub-buffers
       each handling a specific number of lines.
     - The sub-buffers need to be reused.
     - Actually written number of lines needs to be transferred from the write side to the read
       side.
*/
class BufferManager {
public:
    BufferManager(std::size_t max_buffer_size);
    ~BufferManager();

    std::optional<BufferWriteRef>
        get_write(std::size_t first_line, std::size_t last_line, std::size_t line_byte_count);

    std::optional<BufferReadRef> get_read();

    void reset();

private:
    friend class BufferReadRef;
    friend class BufferWriteRef;

    void finish_read(std::size_t index);
    void finish_write(std::size_t index, std::size_t size);

    // Called when there are no available buffers at current write position, so a new one needs
    // to be inserted if the total buffer size is not too large.
    std::optional<BufferWriteRef>
        maybe_insert_for_writing(std::size_t first_line, std::size_t last_line,
                                 std::size_t line_byte_count);

    // Sets up a currently available buffer for writing.
    BufferWriteRef setup_for_writing(std::size_t first_line, std::size_t last_line,
                                     std::size_t line_byte_count);

    void bump_next_read_index();
    void maybe_bump_next_read_index_on_insert();
    void bump_next_write_index();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};


} // namespace sanescan

#endif
