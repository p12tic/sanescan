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

#include "buffer_manager.h"
#include <mutex>
#include <stdexcept>
#include <vector>

namespace sanescan {

BufferReadRef::BufferReadRef(BufferReadRef&& other)
{
    *this = std::move(other);
}

BufferReadRef& BufferReadRef::operator=(BufferReadRef&& other)
{
    manager_ = other.manager_;
    index_ = other.index_;
    data_ = other.data_;
    first_line_ = other.first_line_;
    last_line_ = other.last_line_;
    line_bytes_ = other.line_bytes_;
    finished_ = other.finished_;
    other.finished_ = true;
    return *this;
}

BufferReadRef::~BufferReadRef()
{
    finish();
}

void BufferReadRef::finish()
{
    if (finished_) {
        return;
    }
    finished_ = true;
    manager_->finish_read(index_);
}

BufferWriteRef::BufferWriteRef(BufferWriteRef&& other)
{
    *this = std::move(other);
}

BufferWriteRef& BufferWriteRef::operator=(BufferWriteRef&& other)
{
    manager_ = other.manager_;
    index_ = other.index_;
    data_ = other.data_;
    size_ = other.size_;
    finished_ = other.finished_;
    other.finished_ = true;
    return *this;
}

BufferWriteRef::~BufferWriteRef()
{
    finish(0);
}

void BufferWriteRef::finish(std::size_t size)
{
    if (finished_) {
        return;
    }
    finished_ = true;
    manager_->finish_write(index_, size);
}

struct BufferManagerBuffer
{
    BufferManagerBuffer(std::size_t index, std::size_t requested_size) :
        index{index}
    {
        data.resize(requested_size);
    }

    void setup_for_new_write(std::size_t first_line, std::size_t last_line,
                             std::size_t line_byte_count)
    {
        in_progress = true;
        this->first_line = first_line;
        this->last_line = last_line;
        this->line_byte_count = line_byte_count;
    }

    bool in_progress = false;
    std::size_t index = 0;
    std::vector<char> data;
    std::size_t first_line = 0;
    std::size_t last_line = 0;
    std::size_t line_byte_count = 0;

    // non-copyable because references to BufferManagerBuffer::data may be held
    BufferManagerBuffer(const BufferManagerBuffer&) = delete;
    BufferManagerBuffer& operator=(const BufferManagerBuffer& other) = delete;
};

struct BufferManager::Private {
    std::size_t max_buffer_size = 0;

    std::mutex mutex;

    /*  Buffers is the array of all available buffers. `next_write_index` points to the next
        potentially available buffer for writing. `next_read_index` points to the next potentially
        available buffer for reading. If both are equal then if `has_data` is true then all
        buffers have been written and are potentially available for reading. Otherwise if `has_data`
        is false then all buffers have been read and there are no buffers available for reading.
    */
    std::size_t curr_buffer_size = 0;
    std::size_t next_write_index = 0;
    std::size_t next_read_index = 0;
    bool has_data = false;
    std::vector<std::unique_ptr<BufferManagerBuffer>> buffers;
};

BufferManager::BufferManager(std::size_t max_buffer_size) :
    d_{std::make_unique<Private>()}
{
    d_->max_buffer_size = max_buffer_size;
}

BufferManager::~BufferManager() = default;

std::optional<BufferWriteRef>
    BufferManager::get_write(std::size_t first_line, std::size_t last_line,
                             std::size_t line_byte_count)
{
    std::lock_guard lock{d_->mutex};
    if (d_->next_write_index != d_->next_read_index) {
        if (d_->buffers[d_->next_write_index]->in_progress) {
            return maybe_insert_for_writing(first_line, last_line, line_byte_count);
        } else {
            return setup_for_writing(first_line, last_line, line_byte_count);
        }
    }

    if (d_->has_data || d_->buffers.empty()) {
        return maybe_insert_for_writing(first_line, last_line, line_byte_count);
    }

    if (d_->buffers[d_->next_write_index]->in_progress) {
        return maybe_insert_for_writing(first_line, last_line, line_byte_count);
    } else {
        return setup_for_writing(first_line, last_line, line_byte_count);
    }
}

std::optional<BufferReadRef> BufferManager::get_read()
{
    std::lock_guard lock{d_->mutex};
    if (!d_->has_data) {
        return {};
    }
    if (d_->buffers[d_->next_read_index]->in_progress) {
        return {};
    }

    auto& buffer_ptr = d_->buffers[d_->next_read_index];
    bump_next_read_index();
    buffer_ptr->in_progress = true;

    return BufferReadRef{this, buffer_ptr->index, buffer_ptr->data.data(),
                         buffer_ptr->first_line, buffer_ptr->last_line,
                         buffer_ptr->line_byte_count};
}

void BufferManager::reset()
{
    d_->next_write_index = 0;
    d_->next_read_index = 0;
    d_->has_data = false;
    for (auto& buf_ptr : d_->buffers) {
        buf_ptr->in_progress = false;
    }
}

void BufferManager::finish_read(std::size_t index)
{
    std::lock_guard lock{d_->mutex};
    for (auto& buffer_ptr : d_->buffers) {
        if (buffer_ptr->index == index) {
            if (!buffer_ptr->in_progress) {
                throw std::runtime_error("Attempt to finish already finished buffer");
            }
            buffer_ptr->in_progress = false;
            return;
        }
    }
    throw std::runtime_error("Attempt to finish non-existing buffer");
}

void BufferManager::finish_write(std::size_t index, std::size_t size)
{
    std::lock_guard lock{d_->mutex};
    for (auto& buffer_ptr : d_->buffers) {
        if (buffer_ptr->index == index) {
            if (!buffer_ptr->in_progress) {
                throw std::runtime_error("Attempt to finish already finished buffer");
            }
            buffer_ptr->in_progress = false;

            auto previous_size = (buffer_ptr->last_line - buffer_ptr->first_line) *
                    buffer_ptr->line_byte_count;
            if (previous_size != size) {
                buffer_ptr->last_line = buffer_ptr->first_line + size / buffer_ptr->line_byte_count;
            }
            return;
        }
    }
    throw std::runtime_error("Attempt to finish non-existing buffer");
}

std::optional<BufferWriteRef>
    BufferManager::maybe_insert_for_writing(std::size_t first_line, std::size_t last_line,
                                            std::size_t line_byte_count)
{
    std::size_t requested_size = (last_line - first_line) * line_byte_count;
    if (d_->curr_buffer_size + requested_size > d_->max_buffer_size)
        return {};


    auto insert_pos = d_->buffers.begin() + d_->next_write_index;
    auto ptr_to_insert = std::make_unique<BufferManagerBuffer>(d_->buffers.size(),
                                                               requested_size);

    auto& buffer_ptr = *d_->buffers.insert(insert_pos, std::move(ptr_to_insert));
    d_->curr_buffer_size += requested_size;

    maybe_bump_next_read_index_on_insert();
    bump_next_write_index();

    buffer_ptr->setup_for_new_write(first_line, last_line, line_byte_count);
    return BufferWriteRef{this, buffer_ptr->index, buffer_ptr->data.data(),
                          requested_size};
}

BufferWriteRef BufferManager::setup_for_writing(std::size_t first_line, std::size_t last_line,
                                                std::size_t line_byte_count)
{
    std::size_t requested_size = (last_line - first_line) * line_byte_count;
    auto& buffer_ptr = d_->buffers[d_->next_write_index];
    bump_next_write_index();

    if (buffer_ptr->data.size() < requested_size) {
        d_->curr_buffer_size += requested_size - buffer_ptr->data.size();
        buffer_ptr->data.resize(requested_size);
    }

    buffer_ptr->setup_for_new_write(first_line, last_line, line_byte_count);
    return BufferWriteRef{this, buffer_ptr->index, buffer_ptr->data.data(),
                          requested_size};
}

void BufferManager::bump_next_read_index()
{
    d_->next_read_index++;
    if (d_->next_read_index == d_->buffers.size()) {
        d_->next_read_index = 0;
    }
    if (d_->next_read_index == d_->next_write_index)
        d_->has_data = false;
}

void BufferManager::maybe_bump_next_read_index_on_insert()
{
    if (d_->next_read_index > d_->next_write_index ||
            ((d_->next_read_index == d_->next_write_index) && d_->has_data)) {
        d_->next_read_index++;
    }
}

void BufferManager::bump_next_write_index()
{
    d_->next_write_index++;
    if (d_->next_write_index == d_->buffers.size()) {
        d_->next_write_index = 0;
    }
    d_->has_data = true;
}
} // namespace sanescan
