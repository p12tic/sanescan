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
#include "qimage_converter.h"
#include <deque>
#include <functional>

namespace sanescan {

/// Represents something that can be polled. This is used to interface with SANE wrapper interface
/// which is polling-based.
struct IPoller {
    virtual ~IPoller() {}
    /// Returns true once poll is successful and poller should be destroyed
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
                   QImageConverter* converter) :
        engine{engine},
        wrapper{device_wrapper},
        converter{converter}
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
            converter->add_line(line_index, data, data_size);
        });
        Q_EMIT engine->image_updated();
        return false;
    }

    ScanEngine* engine = nullptr;
    SaneDeviceWrapper* wrapper = nullptr;
    QImageConverter* converter = nullptr;
};

struct ScanEngine::Impl {
    std::vector<std::unique_ptr<IPoller>> pollers;

    SaneWrapper wrapper;
    std::unique_ptr<SaneDeviceWrapper> device_wrapper;
    std::vector<SaneDeviceInfo> current_devices;
    std::vector<SaneOptionGroupDestriptor> option_groups;
    std::map<std::size_t, std::string> option_index_to_name;
    std::map<std::string, std::size_t> option_name_to_index;
    std::map<std::string, SaneOptionValue> option_values;
    bool device_open = false;
    bool scan_active = false;

    SaneParameters params;
    QImageConverter image_converter;
};

ScanEngine::ScanEngine() :
    impl_{std::make_unique<Impl>()}
{}

ScanEngine::~ScanEngine() = default; // TODO: maybe wait for thread

void ScanEngine::perform_step()
{
    // Note that pollers may cause signals to be emitted which may cause additional pollers to be
    // added. As a result we can't use iterators because they may be invalidated whenever poll()
    // is called.
    for (std::size_t i = 0; i < impl_->pollers.size();) {
        if (impl_->pollers[i]->poll()) {
            impl_->pollers.erase(impl_->pollers.begin() + i);
            if (impl_->pollers.empty()) {
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
                impl_->wrapper.get_device_info(),
                [this](auto devices)
    {
        impl_->current_devices = std::move(devices);
        Q_EMIT devices_refreshed();
    }));
}

const std::vector<SaneDeviceInfo>& ScanEngine::current_devices() const
{
    return impl_->current_devices;
}

void ScanEngine::open_device(const std::string& name)
{
    if (impl_->device_open) {
        throw std::runtime_error("Can't open multiple devices concurrently");
    }

    push_poller(std::make_unique<Poller<std::unique_ptr<SaneDeviceWrapper>>>(
                impl_->wrapper.open_device(name),
                [this](auto device_wrapper)
    {
        impl_->device_wrapper = std::move(device_wrapper);
        impl_->device_open = true;
        Q_EMIT device_opened();
        request_options();
        request_option_values();
    }));
}

bool ScanEngine::is_device_opened() const
{
    return impl_->device_open;
}

void ScanEngine::close_device()
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't close already closed device");
    }
    impl_->device_wrapper = nullptr; // this will close device implicitly
    impl_->device_open = false;
    Q_EMIT device_closed();
}

const std::vector<SaneOptionGroupDestriptor>& ScanEngine::get_option_groups() const
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    return impl_->option_groups;
}

const std::map<std::string, SaneOptionValue>& ScanEngine::get_option_values() const
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }
    return impl_->option_values;
}

void ScanEngine::set_option_value(const std::string& name, const SaneOptionValue& value)
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                impl_->device_wrapper->set_option_value(get_option_index(name), value),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::set_option_value_auto(const std::string& name)
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't access options when device is closed");
    }

    push_poller(std::make_unique<Poller<SaneOptionSetInfo>>(
                impl_->device_wrapper->set_option_value_auto(get_option_index(name)),
                [this](SaneOptionSetInfo set_info)
    {
        refresh_after_set_if_needed(set_info);
    }));
}

void ScanEngine::start_scan()
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't control scan when device is closed");
    }
    push_poller(std::make_unique<Poller<SaneParameters>>(
                impl_->device_wrapper->get_parameters(),
                [this](SaneParameters params)
    {
        impl_->params = params;
    }));

    push_poller(std::make_unique<Poller<void>>(
                impl_->device_wrapper->start(),
                [this]()
    {
        impl_->image_converter.start_frame(impl_->params, QColor{0xff, 0xff, 0xff});
        push_poller(std::make_unique<ScanDataPoller>(this, impl_->device_wrapper.get(),
                                                     &impl_->image_converter));
    }));
}

void ScanEngine::cancel_scan()
{
    if (!impl_->device_open) {
        throw std::runtime_error("Can't control scan when device is closed");
    }
}

const QImage& ScanEngine::scan_image() const
{
    return impl_->image_converter.image();
}

void ScanEngine::request_options()
{
    push_poller(std::make_unique<Poller<std::vector<SaneOptionGroupDestriptor>>>(
                impl_->device_wrapper->get_option_groups(),
                [this](auto option_groups)
    {
        impl_->option_groups = std::move(option_groups);
        impl_->option_index_to_name.clear();
        impl_->option_name_to_index.clear();
        for (const auto& group_desc : impl_->option_groups) {
            for (const auto& desc : group_desc.options) {
                impl_->option_index_to_name.emplace(desc.index, desc.name);
                impl_->option_name_to_index.emplace(desc.name, desc.index);
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
                impl_->device_wrapper->get_all_option_values(),
                [this](auto option_values)
    {
        impl_->option_values.clear();
        for (const auto& option : option_values) {
            impl_->option_values.emplace(impl_->option_index_to_name.at(option.index),
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
    auto index_it = impl_->option_name_to_index.find(name);
    if (index_it == impl_->option_name_to_index.end()) {
        throw std::runtime_error("Unknown option: " + name);
    }
    return index_it->second;
}

void ScanEngine::push_poller(std::unique_ptr<IPoller>&& poller)
{
    bool empty = impl_->pollers.empty();
    impl_->pollers.push_back(std::move(poller));
    if (empty) {
        Q_EMIT start_polling();
    }
}

} // namespace sanescan
