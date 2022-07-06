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

#include "scan_engine.h"
#include "../lib/sane_wrapper.h"
#include "../lib/scan_image_buffer.h"
#include <QtGui/QImage>
#include <opencv2/core/mat.hpp>
#include <deque>
#include <functional>

#define SANESCAN_ENGINE_DEBUG_CALLS 0

#if SANESCAN_ENGINE_DEBUG_CALLS
#include <iostream>
#endif

namespace sanescan {

/// Represents something that can be polled. This is used to interface with SANE wrapper interface
/// which is polling-based.
struct IPoller {
    virtual ~IPoller() {}
    /// Returns true once poll is successful and poller should be destroyed. May throw. If poller
    /// returned via thrown exception then the subsequent call to poll() will return true.
    virtual bool poll() = 0;
};

template<class R>
struct Poller : IPoller {
    Poller(std::future<R>&& future, std::function<void(R)> on_value) :
        future{std::move(future)},
        on_value{std::move(on_value)}
    {}

    bool poll() override
    {
        if (!future.valid()) {
            return true;
        }
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) {
            return false;
        }
        on_value(future.get());
        return true;
    }

    std::future<R> future;
    std::function<void(R)> on_value;
};

template<>
struct Poller<void> : IPoller {
    Poller(std::future<void>&& future, std::function<void()> on_value) :
        future{std::move(future)},
        on_value{std::move(on_value)}
    {}

    bool poll() override
    {
        if (!future.valid()) {
            return true;
        }
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) {
            return false;
        }
        future.get();
        on_value();
        return true;
    }

    std::future<void> future;
    std::function<void()> on_value;
};

struct ScanDataPoller : IPoller {
    ScanDataPoller(ScanEngine* engine, SaneDeviceWrapper* device_wrapper,
                   ScanImageBuffer* image_buffer) :
        engine{engine},
        wrapper{device_wrapper},
        image_buffer{image_buffer}
    {}

    bool poll() override
    {
        if (wrapper->finished()) {
            Q_EMIT engine->scan_finished();
            return true;
        }
        wrapper->receive_read_lines([this](std::size_t line_index, const char* data,
                                           std::size_t data_size)
        {
            image_buffer->add_line(line_index, data, data_size);
        });
        Q_EMIT engine->image_updated();
        return false;
    }

    ScanEngine* engine = nullptr;
    SaneDeviceWrapper* wrapper = nullptr;
    ScanImageBuffer* image_buffer = nullptr;
};

struct ScanEngine::Private {
    std::vector<std::unique_ptr<IPoller>> pollers;
    std::vector<std::function<void()>> f_call_when_idle;

    SaneWrapper wrapper;
    std::unique_ptr<SaneDeviceWrapper> device_wrapper;
    std::vector<SaneDeviceInfo> current_devices;
    std::vector<SaneOptionGroupDestriptor> option_groups;
    std::map<std::string, SaneOptionDescriptor> option_descriptors;
    std::map<std::size_t, std::string> option_index_to_name;
    std::map<std::string, std::size_t> option_name_to_index;
    std::map<std::string, SaneOptionValue> option_values;
    bool device_open = false;
    std::string device_name;
    bool scan_active = false;

    SaneParameters params;
    ScanImageBuffer image_buffer;
};

ScanEngine::ScanEngine() :
    d_{std::make_unique<Private>()}
{}

ScanEngine::~ScanEngine() = default; // TODO: maybe wait for thread

void ScanEngine::perform_step()
{
    // Note that pollers may cause signals to be emitted which may cause additional pollers to be
    // added. As a result we can't use iterators because they may be invalidated whenever poll()
    // is called.
    //
    // Also note that polling functions may throw exceptions. We will remove such pollers on next
    // iteration.
    for (std::size_t i = 0; i < d_->pollers.size();) {
        if (d_->pollers[i]->poll()) {
            // We need to attempt invoking call-on-idle functions before pollers array becomes
            // empty because these functions themselves may register call-on-idle functions. These
            // would be invoked immediately which would break the call order.
            maybe_call_idle_functions();

            d_->pollers.erase(d_->pollers.begin() + i);
            if (d_->pollers.empty()) {
                Q_EMIT stop_polling();
            }
        } else {
            ++i;
        }
    }
}

void ScanEngine::refresh_devices()
{
    push_poller(std::make_unique<Poller<std::vector<SaneDeviceInfo>>>(
                d_->wrapper.get_device_info(),
                [this](auto devices)
    {
        d_->current_devices = std::move(devices);
        Q_EMIT devices_refreshed();
    }));
}

const std::vector<SaneDeviceInfo>& ScanEngine::current_devices() const
{
    return d_->current_devices;
}

void ScanEngine::open_device(const std::string& name)
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::open_device: " << name << "\n";
    std::cout.flush();
#endif

    if (d_->device_open) {
        throw std::runtime_error("Can't open multiple devices concurrently");
    }

    push_poller(std::make_unique<Poller<std::unique_ptr<SaneDeviceWrapper>>>(
                d_->wrapper.open_device(name),
                [this, name](auto device_wrapper)
    {
        d_->device_wrapper = std::move(device_wrapper);
        d_->device_open = true;
        d_->device_name = name;
        Q_EMIT device_opened();
        request_options();
        request_option_values();
    }));
}

bool ScanEngine::is_device_opened() const
{
    return d_->device_open;
}

const std::string& ScanEngine::device_name() const
{
    return d_->device_name;
}

void ScanEngine::close_device()
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::close_device: " << d_->device_name << "\n";
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't close already closed device");
    }

    d_->device_wrapper = nullptr; // this will close device implicitly
    d_->device_open = false;
    d_->device_name.clear();
    Q_EMIT device_closed();
}

const std::map<std::string, SaneOptionDescriptor>& ScanEngine::get_option_descriptors() const
{
    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    return d_->option_descriptors;
}

const SaneOptionDescriptor& ScanEngine::get_option_descriptor(const std::string& name) const
{
    const auto& descriptors = get_option_descriptors();
    auto desc_it = descriptors.find(name);
    if (desc_it == descriptors.end()) {
        throw std::runtime_error("Option " + name + "does not exist");
    }
    return desc_it->second;
}

const std::vector<SaneOptionGroupDestriptor>& ScanEngine::get_option_groups() const
{
    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    return d_->option_groups;
}

const std::map<std::string, SaneOptionValue>& ScanEngine::get_option_values() const
{
    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    return d_->option_values;
}

void ScanEngine::set_option_value(const std::string& name, const SaneOptionValue& value)
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::set_option_value: " << name << "=" << value << "\n";
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    const auto& desc = get_option_descriptor(name);
    if (has_flag(desc.cap, SaneCap::INACTIVE)) {
        throw std::runtime_error("Can't set inactive option " + name);
    }
    if (!has_flag(desc.cap, SaneCap::SOFT_SELECT)) {
        throw std::runtime_error("Can't set readonly option " + name);
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                d_->device_wrapper->set_option_value(desc.index, value),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::set_option_value_auto(const std::string& name)
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::set_option_value_auto: " << name << "\n";
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    const auto& desc = get_option_descriptor(name);
    if (has_flag(desc.cap, SaneCap::INACTIVE)) {
        throw std::runtime_error("Can't set inactive option " + name);
    }
    if (!has_flag(desc.cap, SaneCap::SOFT_SELECT)) {
        throw std::runtime_error("Can't set readonly option " + name);
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                d_->device_wrapper->set_option_value_auto(desc.index),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::set_option_values(const std::map<std::string, SaneOptionValue>& values)
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::set_option_value:\n";
    for (const auto& [name, value] : values) {
        std::cout << "    " << name << "=" << value << "\n";
    }
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }

    std::vector<SaneOptionIndexedValue> indexed_values;
    for (const auto& [name, value] : values) {
        const auto& desc = get_option_descriptor(name);
        indexed_values.emplace_back(desc.index, value);
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                d_->device_wrapper->set_option_values(indexed_values),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::start_scan()
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::start_scan\n";
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't control scan when device is closed");
    }
    push_poller(std::make_unique<Poller<SaneParameters>>(
                d_->device_wrapper->get_parameters(),
                [this](SaneParameters params)
    {
        d_->params = params;

        // We want to seup the image as soon as scan parameters are known so that the GUI side can
        // show the image bounds without waiting for some of the scanned data to arrive which can
        // take a while.
        d_->image_buffer.start_frame(d_->params, cv::Scalar(0xff, 0xff, 0xff));
        Q_EMIT image_updated();
    }));

    push_poller(std::make_unique<Poller<void>>(
                d_->device_wrapper->start(),
                [this]()
    {
        push_poller(std::make_unique<ScanDataPoller>(this, d_->device_wrapper.get(),
                                                     &d_->image_buffer));
    }));
}

void ScanEngine::cancel_scan()
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::cancel_scan\n";
    std::cout.flush();
#endif

    if (!d_->device_open) {
        throw std::runtime_error("Can't control scan when device is closed");
    }
}

const cv::Mat& ScanEngine::scan_image() const
{
    return d_->image_buffer.image();
}

void ScanEngine::call_when_idle(std::function<void()> f)
{
    if (d_->pollers.empty()) {
        f();
        return;
    }

    d_->f_call_when_idle.push_back(std::move(f));
}

void ScanEngine::request_options()
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::request_options\n";
    std::cout.flush();
#endif

    push_poller(std::make_unique<Poller<std::vector<SaneOptionGroupDestriptor>>>(
                d_->device_wrapper->get_option_groups(),
                [this](auto option_groups)
    {
        d_->option_groups = std::move(option_groups);
        d_->option_index_to_name.clear();
        d_->option_name_to_index.clear();
        d_->option_descriptors.clear();
        for (const auto& group_desc : d_->option_groups) {
            for (const auto& desc : group_desc.options) {
                d_->option_index_to_name.emplace(desc.index, desc.name);
                d_->option_name_to_index.emplace(desc.name, desc.index);
                d_->option_descriptors.emplace(desc.name, desc);
            }
        }
        Q_EMIT options_changed();
    }));
}

void ScanEngine::request_option_values()
{
#if SANESCAN_ENGINE_DEBUG_CALLS
    std::cout << "ScanEngine::request_option_values\n";
    std::cout.flush();
#endif

    // NOTE: the caller must ensure that request_options is called before this function whenever
    // the parameter list becomes out of date. We don't need to do any additional synchronization
    // here because all requests are processed in order.
    push_poller(std::make_unique<Poller<std::vector<SaneOptionIndexedValue>>>(
                d_->device_wrapper->get_all_option_values(),
                [this](auto option_values)
    {
        d_->option_values.clear();
        for (const auto& option : option_values) {
            d_->option_values.emplace(d_->option_index_to_name.at(option.index),
                                         option.value);
        }
        Q_EMIT option_values_changed();
    }));
}

void ScanEngine::refresh_after_set_if_needed(SaneOptionSetInfo set_info)
{
    if (has_flag(set_info, SaneOptionSetInfo::RELOAD_OPTIONS)) {
        request_options();
    }
    if (has_flag(set_info, SaneOptionSetInfo::INEXACT)) {
        request_option_values();
    }
}

std::size_t ScanEngine::get_option_index(const std::string& name) const
{
    auto index_it = d_->option_name_to_index.find(name);
    if (index_it == d_->option_name_to_index.end()) {
        throw std::runtime_error("Unknown option: " + name);
    }
    return index_it->second;
}

void ScanEngine::push_poller(std::unique_ptr<IPoller>&& poller)
{
    bool empty = d_->pollers.empty();
    d_->pollers.push_back(std::move(poller));
    if (empty) {
        Q_EMIT start_polling();
    }
}

void ScanEngine::maybe_call_idle_functions()
{
    if (d_->pollers.size() != 1) {
        // Most frequent case, handle explicitly.
        return;
    }

    std::size_t called = 0;
    while (d_->pollers.size() == 1 && called < d_->f_call_when_idle.size()) {
        // Note that calling the function may register a new poller or another call-on-idle
        // function, thus we need to recheck the conditions after every call.
        d_->f_call_when_idle[called++]();
    }

    d_->f_call_when_idle.erase(d_->f_call_when_idle.begin(),
                               d_->f_call_when_idle.begin() + called);
}

} // namespace sanescan
