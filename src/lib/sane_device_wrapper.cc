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

#include "sane_device_wrapper.h"
#include "buffer_manager.h"
#include "incomplete_line_manager.h"
#include "sane_exception.h"
#include "sane_utils.h"
#include "task_executor.h"
#include "sane_types_conv.h"
#include <sane/sane.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>

namespace sanescan {

namespace {

int retrieve_option_count(void* handle)
{
    const auto* desc = sane_get_option_descriptor(handle, 0);
    if (desc == nullptr) {
        throw SaneException("Could not get the number of options from the backend");
    }
    int num_options = 0;
    throw_if_sane_status_not_good(sane_control_option(handle, 0, SANE_ACTION_GET_VALUE,
                                                      &num_options, nullptr));
    return num_options;
}

bool is_option_status_no_option(SANE_Status status)
{
    switch (status) {
        case SANE_STATUS_UNSUPPORTED:
        case SANE_STATUS_INVAL:
        case SANE_STATUS_ACCESS_DENIED:
            return true;
        default:
            return false;
    }
}

} // namespace

struct SaneDeviceWrapper::Private {
    // FIXUP: this should be defined dynamically
    static constexpr std::size_t MAX_BUFFER_SIZE = 128 * 1024 * 1024;
    static constexpr std::size_t MAX_SINGLE_READ_SIZE = 128 * 1024;
    static constexpr std::size_t MIN_SINGLE_READ_LINES = 16;

    Private(TaskExecutor* executor, void* handle) :
        executor{executor},
        handle{handle},
        buffer_manager{MAX_BUFFER_SIZE}
    {
    }

    TaskExecutor* executor = nullptr;
    void* handle = nullptr;

    std::atomic<bool> finished = true;
    BufferManager buffer_manager;
    std::exception_ptr read_exception = nullptr;

    // the following variables are supposed to be referenced only from tasks sent to executor
    std::vector<SaneOptionDescriptor> task_option_descriptors;
    SANE_Parameters task_curr_frame_params = {};
    std::size_t task_last_read_line = 0;
    IncompleteLineManager task_partial_line;
};

SaneDeviceWrapper::SaneDeviceWrapper(TaskExecutor* executor, void* handle) :
    d_{std::make_unique<Private>(executor, handle)}
{
}

SaneDeviceWrapper::~SaneDeviceWrapper()
{
    // We do not wait for the device to close because we don't care. All operations are serialized
    // anyway and sane_exit() is guaranteed to happen some time in the future.
    void* handle = d_->handle;
    d_->executor->schedule_task<void>([=]()
    {
        sane_close(handle);
    });
}

std::future<std::vector<SaneOptionGroupDestriptor>>
    SaneDeviceWrapper::get_option_groups()
{
    return d_->executor->schedule_task<std::vector<SaneOptionGroupDestriptor>>([this]()
    {
        return task_get_option_groups();
    });
}

std::future<std::vector<SaneOptionIndexedValue>>
    SaneDeviceWrapper::get_all_option_values()
{
    return d_->executor->schedule_task<std::vector<SaneOptionIndexedValue>>([this]()
    {
        std::vector<SaneOptionIndexedValue> result;
        for (const auto& desc : d_->task_option_descriptors) {
            auto got_option = task_get_option_value(desc);
            if (got_option.has_value()) {
                result.push_back(got_option.value());
            }
        }
        return result;
    });
}

std::future<SaneOptionSetInfo>
    SaneDeviceWrapper::set_option_value(std::size_t index, const SaneOptionValue& value)
{
    return d_->executor->schedule_task<SaneOptionSetInfo>([&, index, value]()
    {
        return task_set_option_value(index, value);
    });
}

std::future<SaneOptionSetInfo>
    SaneDeviceWrapper::set_option_value_auto(std::size_t index)
{
    return d_->executor->schedule_task<SaneOptionSetInfo>([this, index]()
    {
        SANE_Int info = 0;
        throw_if_sane_status_not_good(sane_control_option(d_->handle, index,
                                                          SANE_ACTION_SET_AUTO,
                                                          nullptr, &info));
        return sane_options_info_to_sanescan(info);
    });
}

std::future<SaneOptionSetInfo>
    SaneDeviceWrapper::set_option_values(const std::vector<SaneOptionIndexedValue>& values)
{
    return d_->executor->schedule_task<SaneOptionSetInfo>([this, values]()
    {
        SaneOptionSetInfo combined_status = SaneOptionSetInfo::NONE;

        // Get up to date option group description
        task_get_option_groups();

        // We need to protect against SANE driver continuously requesting us to reload. Worst case
        // it would ask to reload options after each option being set.
        for (int i = 0; i < values.size(); ++i) {
            bool all_set_correctly = true;

            for (const auto& value_index : values) {
                const auto& desc = d_->task_option_descriptors.at(value_index.index);
                if (desc.type == SaneValueType::BUTTON) {
                    continue;
                }
                if (has_flag(desc.cap, SaneCap::INACTIVE) ||
                    !has_flag(desc.cap, SaneCap::SOFT_SELECT))
                {
                    continue;
                }

                // SANE drivers often don't check if value being set has changed. This may cause
                // same option being set repeatedly, RELOAD_OPTIONS being returned and no progress
                // being made.
                auto curr_value = task_get_option_value(desc);
                if (curr_value.has_value() && curr_value.value().value == value_index.value) {
                    continue;
                }

                auto option_status = task_set_option_value(value_index.index, value_index.value);

                combined_status = combined_status | option_status;

                if (has_flag(option_status, SaneOptionSetInfo::RELOAD_OPTIONS)) {
                    task_get_option_groups();
                    all_set_correctly = false;
                    break;
                }
            }

            if (all_set_correctly) {
                break;
            }
        }
        return combined_status;
    });
}

std::future<SaneParameters>
    SaneDeviceWrapper::get_parameters()
{
    return d_->executor->schedule_task<SaneParameters>([this]()
    {
        SANE_Parameters params;
        throw_if_sane_status_not_good(sane_get_parameters(d_->handle, &params));
        return sane_parameters_to_sanescan(params);
    });
}

std::future<void> SaneDeviceWrapper::start()
{
    return d_->executor->schedule_task<void>([this]()
    {
        d_->buffer_manager.reset();
        throw_if_sane_status_not_good(sane_start(d_->handle));
        d_->finished = false;
        task_start_read();
    });
}

void SaneDeviceWrapper::task_start_read()
{
    d_->executor->schedule_task<void>([this]()
    {
        try {
            throw_if_sane_status_not_good(sane_get_parameters(d_->handle,
                                                              &d_->task_curr_frame_params));
            d_->task_last_read_line = 0;
            task_schedule_read();
        }  catch (...) {
            d_->finished = true;
            d_->read_exception = std::current_exception();
        }
    });
}

void SaneDeviceWrapper::task_schedule_read()
{
    auto max_read_lines = compute_read_lines(d_->task_curr_frame_params.bytes_per_line);

    auto first_line = d_->task_last_read_line;
    auto max_last_line = d_->task_curr_frame_params.lines;

    auto read_lines = std::min(max_read_lines, max_last_line - first_line);
    auto last_line = first_line + read_lines;

    d_->executor->schedule_task<void>([this, first_line, last_line]()
    {
        try {
            auto bytes_per_line = d_->task_curr_frame_params.bytes_per_line;
            auto write_buf = d_->buffer_manager.get_write(first_line, last_line, bytes_per_line);

            if (!write_buf.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                task_schedule_read();
                return;
            }

            // sane_read() may read any number of bytes it wants, including zero. That means it
            // may read incomplete line. For these cases we store a partial line in a separate
            // buffer so that write_buf always gets full line.
            auto [buffer, write_size] = d_->task_partial_line.before_read(write_buf->data(),
                                                                          write_buf->size());

            SANE_Int bytes_written = 0;
            auto status = sane_read(d_->handle, reinterpret_cast<SANE_Byte*>(buffer),
                                    write_size, &bytes_written);

            bytes_written = d_->task_partial_line.after_read(buffer, bytes_written,
                                                             bytes_per_line);

            write_buf->finish(bytes_written);

            if (status == SANE_STATUS_EOF || status == SANE_STATUS_CANCELLED) {
                d_->finished = true;
                return;
            }
            throw_if_sane_status_not_good(status);

            // IncompleteLineManager ensures that the number of written bytes is a multiple of
            // per-line byte count.
            d_->task_last_read_line = first_line + bytes_written / bytes_per_line;
            task_schedule_read();
        }  catch (...) {
            d_->finished = true;
            d_->read_exception = std::current_exception();
        }
    });
}

std::size_t SaneDeviceWrapper::compute_read_lines(std::size_t line_bytes)
{
    auto min_lines = Private::MIN_SINGLE_READ_LINES;
    auto max_lines = Private::MAX_SINGLE_READ_SIZE / line_bytes;
    return std::max(min_lines, max_lines);
}

void SaneDeviceWrapper::receive_read_lines(const LineReceivedCallback& on_line_cb)
{
    while (true) {
        auto read_buf = d_->buffer_manager.get_read();
        if (!read_buf.has_value()) {
            return;
        }

        auto data = read_buf->data();
        auto line_bytes = read_buf->line_bytes();

        for (std::size_t line = read_buf->first_line(); line != read_buf->last_line(); ++line) {
            on_line_cb(line, data, line_bytes);
            data += line_bytes;
        }
    }
}

bool SaneDeviceWrapper::finished()
{
    return d_->finished;
}

void SaneDeviceWrapper::cancel()
{
    d_->executor->schedule_task<void>([this]()
    {
        sane_cancel(d_->handle);
    });
}

std::vector<SaneOptionGroupDestriptor> SaneDeviceWrapper::task_get_option_groups()
{
    auto count = retrieve_option_count(d_->handle);

    std::vector<SaneOptionGroupDestriptor> result;
    std::vector<SaneOptionDescriptor> descriptors;
    descriptors.resize(count);

    SaneOptionGroupDestriptor curr_group;
    for (int i = 1; i < count; ++i)
    {
        const auto* desc = sane_get_option_descriptor(d_->handle, i);
        if (desc->type == SANE_TYPE_GROUP) {
            if (!curr_group.options.empty()) {
                result.push_back(std::move(curr_group));
            }

            curr_group = sane_option_descriptor_to_sanescan_group(desc);
        } else {
            auto converted = sane_option_descriptor_to_sanescan(i, desc);
            curr_group.options.push_back(converted);
            descriptors[i] = converted;
        }
    }
    if (!curr_group.options.empty()) {
        result.push_back(std::move(curr_group));
    }

    d_->task_option_descriptors = descriptors;
    return result;
}

std::optional<SaneOptionIndexedValue>
    SaneDeviceWrapper::task_get_option_value(const SaneOptionDescriptor& desc) const
{
    if (has_flag(desc.cap, SaneCap::INACTIVE)) {
        return {};
    }

    switch (desc.type) {
        case SaneValueType::BOOL:{
            std::vector<SANE_Bool> temp;
            temp.resize(desc.size);
            auto status = sane_control_option(d_->handle, desc.index,
                                              SANE_ACTION_GET_VALUE,
                                              temp.data(), nullptr);
            if (is_option_status_no_option(status)) {
                return SaneOptionIndexedValue{desc.index, SaneOptionValueNone{}};
            }
            throw_if_sane_status_not_good(status);

            std::vector<bool> values(temp.begin(), temp.end());
            return SaneOptionIndexedValue{desc.index, std::move(values)};
        }
        case SaneValueType::INT: {
            static_assert(sizeof(SANE_Word) == sizeof(int));
            std::vector<int> values;
            values.resize(desc.size);
            auto status = sane_control_option(d_->handle, desc.index,
                                              SANE_ACTION_GET_VALUE,
                                              values.data(), nullptr);
            if (is_option_status_no_option(status)) {
                return SaneOptionIndexedValue{desc.index, SaneOptionValueNone{}};
                break;
            }
            throw_if_sane_status_not_good(status);

            return SaneOptionIndexedValue{desc.index, std::move(values)};
        }
        case SaneValueType::FLOAT: {
            static_assert(sizeof(SANE_Word) == sizeof(int));
            std::vector<int> temp;
            temp.resize(desc.size);
            auto status = sane_control_option(d_->handle, desc.index,
                                              SANE_ACTION_GET_VALUE,
                                              temp.data(), nullptr);

            if (is_option_status_no_option(status)) {
                return SaneOptionIndexedValue{desc.index, SaneOptionValueNone{}};
            }
            throw_if_sane_status_not_good(status);

            std::vector<double> values;
            values.resize(desc.size);
            for (std::size_t i = 0; i < desc.size; ++i) {
                values[i] = SANE_UNFIX(temp[i]);
            }
            return SaneOptionIndexedValue{desc.index, std::move(values)};
        }
        case SaneValueType::STRING: {
            std::string value;
            value.resize(desc.size);
            auto status = sane_control_option(d_->handle, desc.index,
                                              SANE_ACTION_GET_VALUE,
                                              value.data(), nullptr);

            if (is_option_status_no_option(status)) {
                return SaneOptionIndexedValue{desc.index, SaneOptionValueNone{}};
            }
            throw_if_sane_status_not_good(status);

            value.resize(std::strlen(value.c_str()));
            return SaneOptionIndexedValue{desc.index, std::move(value)};
        }
        default: {
            // Button and group options don't have values.
            return {};
        }
    }
}

SaneOptionSetInfo SaneDeviceWrapper::task_set_option_value(std::size_t index,
                                                           const SaneOptionValue& value) const
{
    SANE_Int info = 0;

    if (std::get_if<SaneOptionValueNone>(&value.value)) {
        throw std::invalid_argument("Option None is invalid in set_option_value");
    }

    // note that we expect the caller to send correct data type for the option
    const auto* bool_values = value.as_bool_vector();
    if (bool_values) {
        std::vector<SANE_Word> temp;

        // implicit conversion from bool to word will do the right thing
        temp.assign(bool_values->begin(), bool_values->end());
        throw_if_sane_status_not_good(sane_control_option(d_->handle, index,
                                                          SANE_ACTION_SET_VALUE,
                                                          temp.data(), &info));
        return sane_options_info_to_sanescan(info);
    }

    const auto* int_values = value.as_int_vector();
    if (int_values) {
        static_assert(sizeof(SANE_Word) == sizeof(int));
        void* ptr = const_cast<void*>(static_cast<const void*>(int_values->data()));
        throw_if_sane_status_not_good(sane_control_option(d_->handle, index,
                                                          SANE_ACTION_SET_VALUE,
                                                          ptr, &info));
        return sane_options_info_to_sanescan(info);
    }

    const auto* double_values = value.as_double_vector();
    if (double_values) {
        std::vector<SANE_Word> temp;
        temp.resize(double_values->size());
        for (std::size_t i = 0; i < temp.size(); ++i) {
            temp[i] = SANE_FIX((*double_values)[i]);
        }
        throw_if_sane_status_not_good(sane_control_option(d_->handle, index,
                                                          SANE_ACTION_SET_VALUE,
                                                          temp.data(), &info));
        return sane_options_info_to_sanescan(info);
    }

    const auto* string = value.as_string();
    if (string) {
        void* ptr = const_cast<void*>(static_cast<const void*>(string->c_str()));
        throw_if_sane_status_not_good(sane_control_option(d_->handle, index,
                                                          SANE_ACTION_SET_VALUE,
                                                          ptr, &info));
        return sane_options_info_to_sanescan(info);
    }

    throw SaneException("Option variant is expected to carry option value");
}


} // namespace sanescan
