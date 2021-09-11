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
#include "sane_exception.h"
#include "sane_utils.h"
#include "task_executor.h"
#include "sane_types_conv.h"
#include <sane/sane.h>
#include <cstring>

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

int convert_sane_option_size(SANE_Value_Type type, int size)
{
    switch (type) {
        case SANE_TYPE_BOOL:
        case SANE_TYPE_INT:
        case SANE_TYPE_FIXED:
            return size / sizeof(SANE_Word);
        default:
            return size;
    }
}

SaneOptionDescriptor convert_sane_option_descriptor(int index, const SANE_Option_Descriptor* desc)
{
    SaneOptionDescriptor option;
    option.index = index;
    option.name = desc->name;
    option.title = desc->title;
    option.description = desc->desc;
    option.unit = sane_unit_to_sanescan(desc->unit);
    option.type = sane_value_type_to_sanescan(desc->type);
    option.size = convert_sane_option_size(desc->type, desc->size);
    option.cap = sane_cap_to_sanescan(desc->cap);

    switch (desc->constraint_type) {
        default:
        case SANE_CONSTRAINT_NONE: {
            option.constraint = SaneConstraintNone{};
            break;
        }
        case SANE_CONSTRAINT_RANGE: {
            const auto* range = desc->constraint.range;
            switch (option.type) {
                case SaneValueType::INT: {
                    option.constraint = SaneConstraintIntRange{range->min, range->max, range->quant};
                    break;
                }
                case SaneValueType::FLOAT: {
                    option.constraint = SaneConstraintFloatRange{SANE_UNFIX(range->min),
                                                                 SANE_UNFIX(range->max),
                                                                 SANE_UNFIX(range->quant)};
                    break;
                }
                default:
                    throw SaneException("word list constraint used on wrong option type " +
                                        std::to_string(desc->type));
            }
            break;
        }
        case SANE_CONSTRAINT_STRING_LIST: {
            const SANE_String_Const* ptr = desc->constraint.string_list;
            SaneConstraintStringList constraint;
            while (*ptr != nullptr) {
                constraint.strings.push_back(*ptr++);
            }
            option.constraint = std::move(constraint);
            break;
        }
        case SANE_CONSTRAINT_WORD_LIST: {
            const SANE_Word* ptr = desc->constraint.word_list;
            switch (option.type) {
                case SaneValueType::INT: {
                    SaneConstraintIntList constraint;
                    int length = *ptr++;
                    constraint.numbers.assign(ptr, ptr + length);
                    option.constraint = std::move(constraint);
                    break;
                }
                case SaneValueType::FLOAT: {
                    SaneConstraintFloatList constraint;
                    int length = *ptr++;
                    constraint.numbers.reserve(length);
                    for (int i = 0; i < length; ++i) {
                        constraint.numbers.push_back(SANE_UNFIX(*ptr++));
                    }
                    option.constraint = std::move(constraint);
                    break;
                }
                default:
                    throw SaneException("word list constraint used on wrong option type " +
                                        std::to_string(desc->type));
            }
            break;
        }
    }
    return option;
}

SaneOptionGroupDestriptor convert_sane_option_group_descriptor(const SANE_Option_Descriptor* desc)
{
    SaneOptionGroupDestriptor option_group;
    option_group.name = desc->name;
    option_group.title = desc->title;
    option_group.description = desc->desc;
    return option_group;
}

} // namespace

struct SaneDeviceWrapper::Impl {
    // FIXUP: this should be defined dynamically
    static constexpr std::size_t MAX_BUFFER_SIZE = 128 * 1024 * 1024;
    static constexpr std::size_t MAX_SINGLE_READ_SIZE = 128 * 1024;
    static constexpr std::size_t MIN_SINGLE_READ_LINES = 16;

    Impl(TaskExecutor* executor, void* handle) :
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
    std::vector<SaneOptionGroupDestriptor> task_option_descriptors;
    SANE_Parameters task_curr_frame_params = {};
    std::size_t task_last_read_line = 0;
};

SaneDeviceWrapper::SaneDeviceWrapper(TaskExecutor* executor, void* handle) :
    impl_{std::make_unique<Impl>(executor, handle)}
{
}

SaneDeviceWrapper::~SaneDeviceWrapper()
{
    // We do not wait for the device to close because we don't care. All operations are serialized
    // anyway and sane_exit() is guaranteed to happen some time in the future.
    void* handle = impl_->handle;
    impl_->executor->schedule_task<void>([=]()
    {
        sane_close(handle);
    });
}

std::future<std::vector<SaneOptionGroupDestriptor>>
    SaneDeviceWrapper::get_option_groups()
{
    return impl_->executor->schedule_task<std::vector<SaneOptionGroupDestriptor>>([this]()
    {
        auto count = retrieve_option_count(impl_->handle);

        std::vector<SaneOptionGroupDestriptor> result;

        SaneOptionGroupDestriptor curr_group;
        for (int i = 1; i < count; ++i)
        {
            const auto* desc = sane_get_option_descriptor(impl_->handle, i);
            if (desc->type == SANE_TYPE_GROUP) {
                if (!curr_group.options.empty()) {
                    result.push_back(std::move(curr_group));
                }

                curr_group = convert_sane_option_group_descriptor(desc);
            } else {
                curr_group.options.push_back(convert_sane_option_descriptor(i, desc));
            }
        }
        if (!curr_group.options.empty()) {
            result.push_back(std::move(curr_group));
        }

        impl_->task_option_descriptors = result;
        return result;
    });
}

std::future<std::vector<SaneOptionIndexedValue>>
    SaneDeviceWrapper::get_all_option_values()
{
    return impl_->executor->schedule_task<std::vector<SaneOptionIndexedValue>>([this]()
    {
        std::vector<SaneOptionIndexedValue> result;
        for (const auto& group_desc : impl_->task_option_descriptors) {
            for (const auto& desc : group_desc.options) {
                switch (desc.type) {
                    case SaneValueType::BOOL:
                    case SaneValueType::BUTTON: {
                        std::vector<SANE_Bool> temp;
                        temp.resize(desc.size);
                        throw_if_sane_status_not_good(sane_control_option(impl_->handle, desc.index,
                                                                          SANE_ACTION_GET_VALUE,
                                                                          temp.data(), nullptr));
                        std::vector<bool> values(temp.begin(), temp.end());
                        result.emplace_back(desc.index, std::move(values));
                        break;
                    }
                    case SaneValueType::INT: {
                        static_assert(sizeof(SANE_Word) == sizeof(int));
                        std::vector<int> values;
                        values.resize(desc.size);
                        throw_if_sane_status_not_good(sane_control_option(impl_->handle, desc.index,
                                                                          SANE_ACTION_GET_VALUE,
                                                                          values.data(), nullptr));
                        result.emplace_back(desc.index, std::move(values));
                        break;
                    }
                    case SaneValueType::FLOAT: {
                        static_assert(sizeof(SANE_Word) == sizeof(int));
                        std::vector<int> temp;
                        temp.resize(desc.size);
                        throw_if_sane_status_not_good(sane_control_option(impl_->handle, desc.index,
                                                                          SANE_ACTION_GET_VALUE,
                                                                          temp.data(), nullptr));
                        std::vector<double> values;
                        values.resize(desc.size);
                        for (std::size_t i = 0; i < desc.size; ++i) {
                            values[i] = SANE_UNFIX(temp[i]);
                        }
                        result.emplace_back(desc.index, std::move(values));
                        break;
                    }
                    case SaneValueType::STRING: {
                        std::string value;
                        value.resize(desc.size);
                        throw_if_sane_status_not_good(sane_control_option(impl_->handle, desc.index,
                                                                          SANE_ACTION_GET_VALUE,
                                                                          value.data(), nullptr));
                        value.resize(std::strlen(value.c_str()));
                        result.emplace_back(desc.index, std::move(value));
                        break;
                    }
                    case SaneValueType::GROUP: {
                        throw new SaneException("Unexpected option group when retrieving values");
                    }
                }

            }
        }
        return result;
    });
}

std::future<SaneOptionSetInfo>
    SaneDeviceWrapper::set_option_value(std::size_t index, const SaneOptionValue& value)
{
    return impl_->executor->schedule_task<SaneOptionSetInfo>([&, index, value]()
    {
        SANE_Int info = 0;

        // note that we expect the caller to send correct data type for the option
        const auto* bool_values = std::get_if<std::vector<bool>>(&value);
        if (bool_values) {
            std::vector<SANE_Word> temp;

            // implicit conversion from bool to word will do the right thing
            temp.assign(bool_values->begin(), bool_values->end());
            throw_if_sane_status_not_good(sane_control_option(impl_->handle, index,
                                                              SANE_ACTION_SET_VALUE,
                                                              temp.data(), &info));
            return sane_options_info_to_sanescan(info);
        }

        const auto* int_values = std::get_if<std::vector<int>>(&value);
        if (int_values) {
            static_assert(sizeof(SANE_Word) == sizeof(int));
            void* ptr = const_cast<void*>(static_cast<const void*>(int_values->data()));
            throw_if_sane_status_not_good(sane_control_option(impl_->handle, index,
                                                              SANE_ACTION_SET_VALUE,
                                                              ptr, &info));
            return sane_options_info_to_sanescan(info);
        }

        const auto* double_values = std::get_if<std::vector<double>>(&value);
        if (double_values) {
            std::vector<SANE_Word> temp;
            temp.resize(double_values->size());
            for (std::size_t i = 0; i < temp.size(); ++i) {
                temp[i] = SANE_FIX((*double_values)[i]);
            }
            throw_if_sane_status_not_good(sane_control_option(impl_->handle, index,
                                                              SANE_ACTION_SET_VALUE,
                                                              temp.data(), &info));
            return sane_options_info_to_sanescan(info);
        }

        const auto* string = std::get_if<std::string>(&value);
        if (string) {
            void* ptr = const_cast<void*>(static_cast<const void*>(string->c_str()));
            throw_if_sane_status_not_good(sane_control_option(impl_->handle, index,
                                                              SANE_ACTION_SET_VALUE,
                                                              ptr, &info));
            return sane_options_info_to_sanescan(info);
        }

        throw SaneException("Option variant is expected to carry option value");
    });
}

std::future<SaneOptionSetInfo>
    SaneDeviceWrapper::set_option_value_auto(std::size_t index)
{
    return impl_->executor->schedule_task<SaneOptionSetInfo>([this, index]()
    {
        SANE_Int info = 0;
        throw_if_sane_status_not_good(sane_control_option(impl_->handle, index,
                                                          SANE_ACTION_SET_AUTO,
                                                          nullptr, &info));
        return sane_options_info_to_sanescan(info);
    });
}

std::future<SaneParameters>
    SaneDeviceWrapper::get_parameters()
{
    return impl_->executor->schedule_task<SaneParameters>([this]()
    {
        SANE_Parameters params;
        throw_if_sane_status_not_good(sane_get_parameters(impl_->handle, &params));

        SaneParameters result;
        result.frame = sane_frame_type_to_sanescan(params.format);
        result.last_frame = params.last_frame;
        result.bytes_per_line = params.bytes_per_line;
        result.pixels_per_line = params.pixels_per_line;
        result.lines = params.lines;
        result.depth = params.depth;
        return result;
    });
}

std::future<void> SaneDeviceWrapper::start()
{
    return impl_->executor->schedule_task<void>([this]()
    {
        impl_->buffer_manager.reset();
        throw_if_sane_status_not_good(sane_start(impl_->handle));
        impl_->finished = false;
        task_start_read();
    });
}

void SaneDeviceWrapper::task_start_read()
{
    impl_->executor->schedule_task<void>([this]()
    {
        try {
            throw_if_sane_status_not_good(sane_get_parameters(impl_->handle,
                                                              &impl_->task_curr_frame_params));
            impl_->task_last_read_line = 0;
            task_schedule_read();
        }  catch (...) {
            impl_->finished = true;
            impl_->read_exception = std::current_exception();
        }
    });
}

void SaneDeviceWrapper::task_schedule_read()
{
    auto max_read_lines = compute_read_lines(impl_->task_curr_frame_params.bytes_per_line);

    auto first_line = impl_->task_last_read_line;
    auto max_last_line = impl_->task_curr_frame_params.lines;

    auto read_lines = std::min(max_read_lines, max_last_line - first_line);
    auto last_line = first_line + read_lines;

    impl_->executor->schedule_task<void>([=]()
    {
        try {
            auto bytes_per_line = impl_->task_curr_frame_params.bytes_per_line;
            auto write_buf = impl_->buffer_manager.get_write(first_line, last_line, bytes_per_line);

            if (!write_buf.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                task_schedule_read();
                return;
            }

            SANE_Int bytes_written = 0;
            auto status = sane_read(impl_->handle, reinterpret_cast<SANE_Byte*>(write_buf->data()),
                                    write_buf->size(), &bytes_written);
            write_buf->finish(bytes_written);

            if (status == SANE_STATUS_EOF || status == SANE_STATUS_CANCELLED) {
                impl_->finished = true;
                return;
            }
            throw_if_sane_status_not_good(status);

            impl_->task_last_read_line = last_line;
            task_schedule_read();
        }  catch (...) {
            impl_->finished = true;
            impl_->read_exception = std::current_exception();
        }
    });
}

std::size_t SaneDeviceWrapper::compute_read_lines(std::size_t line_bytes)
{
    auto min_lines = Impl::MIN_SINGLE_READ_LINES;
    auto max_lines = Impl::MAX_SINGLE_READ_SIZE / line_bytes;
    return std::max(min_lines, max_lines);
}

void SaneDeviceWrapper::receive_read_lines(const LineReceivedCallback& on_line_cb)
{
    while (true) {
        auto read_buf = impl_->buffer_manager.get_read();
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
    return impl_->finished;
}

void SaneDeviceWrapper::cancel()
{
    impl_->executor->schedule_task<void>([this]()
    {
        sane_cancel(impl_->handle);
    });
}

} // namespace sanescan
