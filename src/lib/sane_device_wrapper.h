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

#ifndef SANESCAN_LIB_SANE_DEVICE_WRAPPER_H
#define SANESCAN_LIB_SANE_DEVICE_WRAPPER_H

#include <future>
#include <vector>
#include "sane_types.h"
#include "fwd.h"

namespace sanescan {

/** Corresponds to SANE_Handle. Operations on multiple SaneDeviceWrapper instances happen in
    serial order even if executed from multiple threads.
*/
class SaneDeviceWrapper {
public:
    using LineReceivedCallback = std::function<void(std::size_t line_index,
                                                    const char* data,
                                                    std::size_t data_size)>;

    /** Creates a SANE device wrapper for the given SANE_Handle. All SANE operations will be done
        through the given task executor.
    */
    SaneDeviceWrapper(TaskExecutor* executor, void* handle);

    /// Descroying the instance will close the associated SANE device
    ~SaneDeviceWrapper();

    /// The option that contains the total option count is not returned
    std::future<std::vector<SaneOptionGroupDestriptor>> get_option_groups();

    /// The option that contains the total option count is not returned
    std::future<std::vector<SaneOptionIndexedValue>> get_all_option_values();
    std::future<SaneOptionSetInfo> set_option_value(std::size_t index,
                                                    const SaneOptionValue& value);
    std::future<SaneOptionSetInfo> set_option_value_auto(std::size_t index);

    /** Sets option values. This function handles the case when certain options depend on other
        options being enabled. In such case options are set in appropriate order so that first
        options are enabled and then set to appropriate values.

        Options of button type are ignored.
    */
    std::future<SaneOptionSetInfo>
        set_option_values(const std::vector<SaneOptionIndexedValue>& values);

    std::future<SaneParameters> get_parameters();

    std::future<void> start();

    /// Returns currently read lines through the supplied callback
    void receive_read_lines(const LineReceivedCallback& on_line_cb);
    bool finished();
    void cancel();

private:
    friend class SaneWrapper;

    void task_start_read();
    void task_schedule_read();
    static std::size_t compute_read_lines(std::size_t line_bytes);

    std::vector<SaneOptionGroupDestriptor> task_get_option_groups();
    std::optional<SaneOptionIndexedValue>
        task_get_option_value(const SaneOptionDescriptor& desc) const;
    SaneOptionSetInfo task_set_option_value(std::size_t index, const SaneOptionValue& value) const;

private:
    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif
