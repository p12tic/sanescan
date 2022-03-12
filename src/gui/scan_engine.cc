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

    SaneWrapper wrapper;
    std::unique_ptr<SaneDeviceWrapper> device_wrapper;
    std::vector<SaneDeviceInfo> current_devices;
    std::vector<SaneOptionGroupDestriptor> option_groups;
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
    if (!d_->device_open) {
        throw std::runtime_error("Can't close already closed device");
    }
    d_->device_wrapper = nullptr; // this will close device implicitly
    d_->device_open = false;
    d_->device_name.clear();
    Q_EMIT device_closed();
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
    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                d_->device_wrapper->set_option_value(get_option_index(name), value),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::set_option_value_auto(const std::string& name)
{
    if (!d_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                d_->device_wrapper->set_option_value_auto(get_option_index(name)),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::start_scan()
{
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
    if (!d_->device_open) {
        throw std::runtime_error("Can't control scan when device is closed");
    }
}

const cv::Mat& ScanEngine::scan_image() const
{
    return d_->image_buffer.image();
}

void ScanEngine::request_options()
{
    push_poller(std::make_unique<Poller<std::vector<SaneOptionGroupDestriptor>>>(
                d_->device_wrapper->get_option_groups(),
                [this](auto option_groups)
    {
        d_->option_groups = std::move(option_groups);
        d_->option_index_to_name.clear();
        d_->option_name_to_index.clear();
        for (const auto& group_desc : d_->option_groups) {
            for (const auto& desc : group_desc.options) {
                d_->option_index_to_name.emplace(desc.index, desc.name);
                d_->option_name_to_index.emplace(desc.name, desc.index);
            }
        }
        Q_EMIT options_changed();
    }));
}

void ScanEngine::request_option_values()
{
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
    if ((set_info & SaneOptionSetInfo::RELOAD_PARAMS) != SaneOptionSetInfo::NONE) {
        request_options();
    }
    if ((set_info & (SaneOptionSetInfo::RELOAD_OPTIONS | SaneOptionSetInfo::INEXACT)) !=
        SaneOptionSetInfo::NONE)
    {
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

} // namespace sanescan
