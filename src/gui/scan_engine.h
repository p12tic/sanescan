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

#ifndef SANESCAN_GUI_SCAN_ENGINE_H
#define SANESCAN_GUI_SCAN_ENGINE_H

#include "../lib/sane_types.h"
#include <QtCore/QObject>
#include <memory>

namespace sanescan {

struct IPoller;

/** This class is the main point where the Qt GUI and asynchronous SANE wrapper meet. It hooks into
    the GUI via the `perform_step()` function which is supposed to be called repeatedly and
    then fires signals whenever an important event has occurred.

    Currently only single scan is supported concurrently.

    `start_polling()` and `end_polling()` signals are emitted when the engine starts and ends
    polling respectively. They can be used to control how often `perform_step()` is called, if at
    all.
*/
class ScanEngine : public QObject {
    Q_OBJECT
public:
    ScanEngine();
    ~ScanEngine() override;

    void perform_step();

    /** Issues a command to refresh devices. Once this is finished, `devices_refreshed` signal is
        emitted and `current_devices` will contain updated values
    */
    void refresh_devices();

    const std::vector<SaneDeviceInfo>& current_devices() const;

    /** Issues a command to open a device. Once this is finished, `device_opened` signal is emitted
        and `is_device_opened()` will return true.

        The options available to the scanner are refreshed automatically and will cause
        `options_changed` and `option_values_changed` signals to be emitted.
    */
    void open_device(const std::string& name);
    bool is_device_opened() const;

    /** Issues a command to close a device. Once this is finished, `device_closed` signal is
        emitted. `is_device_opened()` will return false immediately after this command is issued.
    */
    void close_device();

    /// Returns option descriptors for current device
    const std::vector<SaneOptionGroupDestriptor>& get_option_groups() const;

    /// Returns current option values
    const std::map<std::string, SaneOptionValue>& get_option_values() const;

    /// Sets option value. Once the request finishes, `options_changed` signal will be emitted.
    void set_option_value(const std::string& name, const SaneOptionValue& value);
    void set_option_value_auto(const std::string& name);

    /** Starts a scan. Once a scan is finished, `scan_finished` signal will be emitted. Whenever
        scan image is updated, `image_updated` signal will be emitted.
    */
    void start_scan();

    /// Cancels a scan. `scan_finished` signal will be emitted once the request completes.
    void cancel_scan();

    /// Returns the current state of the scanned image.
    /// FIXME: this API will change once image data sharing is figured out
    const QImage& scan_image() const;

Q_SIGNALS:
    void devices_refreshed();
    void device_opened();
    void device_closed();
    void options_changed();
    void option_values_changed();
    void scan_finished();
    void image_updated();
    void on_error(const std::string& error_message);

    void start_polling();
    void stop_polling();

private:
    void request_options();
    void request_option_values();
    void refresh_after_set_if_needed(SaneOptionSetInfo set_info);
    std::size_t get_option_index(const std::string& name) const;
    void push_poller(std::unique_ptr<IPoller>&& poller);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_SCAN_ENGINE_H