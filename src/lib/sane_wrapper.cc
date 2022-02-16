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

#include "sane_wrapper.h"
#include "task_executor.h"
#include "sane_utils.h"
#include <sane/sane.h>

namespace sanescan {

struct SaneWrapper::Private
{
    TaskExecutor executor;
    bool initialized = false;
};

SaneWrapper::SaneWrapper() : d_{std::make_unique<Private>()}
{
    auto fut = d_->executor.schedule_task<void>([]()
    {
        throw_if_sane_status_not_good(sane_init(nullptr, nullptr));
    });
    fut.wait();
    d_->initialized = true;
}

SaneWrapper::~SaneWrapper()
{
    if (d_->initialized) {
        auto fut = d_->executor.schedule_task<void>([]()
        {
            sane_exit();
        });
        fut.wait();
    }
}

std::future<std::vector<SaneDeviceInfo>> SaneWrapper::get_device_info()
{
    return d_->executor.schedule_task<std::vector<SaneDeviceInfo>>([]()
    {
        const SANE_Device** devices;
        throw_if_sane_status_not_good(sane_get_devices(&devices, true));

        std::vector<SaneDeviceInfo> result;
        while (*devices != nullptr) {
            SaneDeviceInfo info;
            info.model = (*devices)->model;
            info.vendor = (*devices)->vendor;
            info.type = (*devices)->type;
            info.name = (*devices)->name;
            result.push_back(info);
            devices++;
        }
        return result;
    });
}

std::future<std::unique_ptr<SaneDeviceWrapper>> SaneWrapper::open_device(const std::string& name)
{
    return d_->executor.schedule_task<std::unique_ptr<SaneDeviceWrapper>>([this, name]()
    {
        SANE_Handle handle = nullptr;
        throw_if_sane_status_not_good(sane_open(name.c_str(), &handle));
        return std::make_unique<SaneDeviceWrapper>(&d_->executor, handle);
    });
}

} // namespace sanescan

