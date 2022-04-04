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

#ifndef SANESCAN_GUI_DOCUMENT_MANAGER_H
#define SANESCAN_GUI_DOCUMENT_MANAGER_H

#include "scan_document.h"
#include <QtCore/QObject>
#include <memory>

namespace sanescan {

class DocumentManager : public QObject {
    Q_OBJECT
public:
    DocumentManager();
    ~DocumentManager() override;

    /// Instructs the underlying engine to refresh available devices
    void refresh_devices();

    /// Returns currently available devices as seen by the underlying engine
    const std::vector<SaneDeviceInfo>& available_devices();

    /** Selects device for a particular document. Currently only document with
        curr_scan_document_index can change the selected device
    */
    void select_device(unsigned doc_index, const std::string& name);

    /** Starts scan for a particular document. If the document is not curr_scan_document_index then
        the scan settings are transferred to curr_scan_document_index document and scanning is
        started.
    */
    void start_scan(unsigned doc_index);

    /** Sets option for a particular document. Currently only document with curr_scan_document_index
        can have its options changed.
    */
    void set_document_option(unsigned doc_index, const std::string& name,
                             const SaneOptionValue& value);

    /// Returns document at particular index
    const ScanDocument& document(unsigned doc_index) const;

    /// Returns total document count
    unsigned document_count() const;

    /** Returns the document that is currently prepared for scan. The options set to this document
        are synchronized with the underlying scan engine.
    */
    unsigned curr_scan_document_index() const;

    /** Returns whether all documents should be considered locked regardless of their status stored
        in the `locked` attribute.
    */
    bool are_documents_globally_locked() const;

public: Q_SIGNALS:
    void available_devices_changed();
    void new_document_added(unsigned doc_index, bool after_scan);
    void document_option_descriptors_changed(unsigned doc_index);
    void document_option_values_changed(unsigned doc_index);
    void document_scan_progress_changed(unsigned doc_index);
    void document_image_changed(unsigned doc_index);
    void document_preview_image_changed(unsigned doc_index);
    void document_locking_changed();

private:
    void reopen_current_device();
    const SaneDeviceInfo& get_available_device_by_name(const std::string& name);

    ScanDocument& curr_scan_document();
    void setup_empty_preview_image(ScanDocument& document,
                                   const std::optional<QRectF>& scan_bounds_mm);
    void clear_preview_image(ScanDocument& document);

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

#endif // SANESCAN_GUI_DOCUMENT_MANAGER_H
