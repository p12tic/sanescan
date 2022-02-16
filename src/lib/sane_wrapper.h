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

#ifndef SANESCAN_LIB_SANE_WRAPPER_H
#define SANESCAN_LIB_SANE_WRAPPER_H

#include <string>
#include <string_view>
#include <future>
#include <variant>
#include <vector>
#include "fwd.h"
#include "sane_types.h"
#include "sane_device_wrapper.h"

namespace sanescan {

/** Interacting with a SANE backend may take an unspecified amount of time, so all operations
    are hidden behind an asynchronous interface. Any number of tasks can be in flight at any given
    time: the underlying implementation will serialize everything to a single thread.

    SaneWrapper is the entry point to all functionality exposed by SANE.
*/
class SaneWrapper {
public:
    SaneWrapper();
    ~SaneWrapper();

    std::future<std::vector<SaneDeviceInfo>> get_device_info();

    // opens a device with specific name (see SaneDeviceInfo::name). The returned device must
    // be destroyed before SaneWrapper
    std::future<std::unique_ptr<SaneDeviceWrapper>> open_device(const std::string& name);
private:

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif
