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

#ifndef SANESCAN_GUI_PAGE_MANAGER_H
#define SANESCAN_GUI_PAGE_MANAGER_H

#include "scan_page.h"
#include <QtCore/QObject>
#include <memory>

namespace sanescan {

class PageManager : public QObject {
    Q_OBJECT
public:
    PageManager();
    ~PageManager() override;

    /// Instructs the underlying engine to refresh available devices
    void refresh_devices();

    /// Returns currently available devices as seen by the underlying engine
    const std::vector<SaneDeviceInfo>& available_devices();

    /** Selects device for a particular page. Currently only page with
        curr_scan_page_index can change the selected device
    */
    void select_device(unsigned page_index, const std::string& name);

    /** Starts scan for a particular page. If the page is not curr_scan_page_index then
        the scan settings are transferred to curr_scan_page_index page and scanning is
        started.
    */
    void start_scan(unsigned page_index, ScanType type);

    /** Sets option for a particular page. Currently only page with curr_scan_page_index
        can have its options changed.
    */
    void set_page_option(unsigned page_index, const std::string& name,
                         const SaneOptionValue& value);

    /// Returns page at particular index
    const ScanPage& page(unsigned page_index) const;

    /// Returns total page count
    unsigned page_count() const;

    /** Returns the page that is currently prepared for scan. The options set to this page
        are synchronized with the underlying scan engine.
    */
    unsigned curr_scan_page_index() const;

    /** Returns whether all pages should be considered locked regardless of their status stored
        in the `locked` attribute.
    */
    bool are_pages_globally_locked() const;

    /// Sets OCR options for specific page and restarts OCR processing if needed
    void set_page_ocr_options(unsigned page_index, const OcrOptions& options);

public: Q_SIGNALS:
    void available_devices_changed();
    void new_page_added(unsigned page_index, bool after_scan);
    void page_option_descriptors_changed(unsigned page_index);
    void page_option_values_changed(unsigned page_index);
    void page_scan_progress_changed(unsigned page_index);
    void page_image_changed(unsigned page_index);
    void page_preview_image_changed(unsigned page_index);
    void page_locking_changed();

    /// emitted when either ocr_results or ocr_progress of a page changes.
    void page_ocr_results_changed(unsigned page_index);

private Q_SLOTS:
    void on_ocr_complete(unsigned page_index);

private:
    void reopen_current_device();
    const SaneDeviceInfo& get_available_device_by_name(const std::string& name);

    ScanPage& curr_scan_page();
    void setup_empty_preview_image(ScanPage& page,
                                   const std::optional<cv::Rect2d>& scan_bounds_mm);
    void clear_preview_image(ScanPage& page);
    void perform_ocr(unsigned page_index);

    void periodic_engine_poll();
    void devices_refreshed();
    void options_changed();
    void option_values_changed();
    void device_opened();
    void device_closed();
    void image_updated();
    void scan_finished();

    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_PAGE_MANAGER_H
