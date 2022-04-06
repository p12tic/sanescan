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

#include "document_manager.h"
#include "scan_engine.h"
#include "lib/scan_area_utils.h"
#include "util/math.h"

#include <QtCore/QTimer>
#include <QtGui/QImage>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace sanescan {

namespace {

PreviewConfig get_default_preview_config()
{
    // Use A4 size by default. At the given dpi the blank image is relatively small at 163x233
    // pixels
    return PreviewConfig{210, 297, 20};
}

PreviewConfig setup_blank_preview_size(const std::optional<cv::Rect2d>& bounds_mm)
{
    if (!bounds_mm.has_value()) {
        return get_default_preview_config();
    }

    double width_mm = bounds_mm->width;
    double height_mm = bounds_mm->height;

    // Do some checking against scanners returning useless size (e.g. one dimension much larger
    // than the other). In such case the user would need to do a preview scan anyway because
    // we can't display it properly.
    double MAX_RELATIVE_SIZE_DIFF = 10;
    if (width_mm > height_mm * MAX_RELATIVE_SIZE_DIFF ||
        height_mm > width_mm * MAX_RELATIVE_SIZE_DIFF)
    {
        return get_default_preview_config();
    }

    double dpi = 50;
    double dots_per_mm = dpi / 25.4;

    // Look at the scanner size and set appropriate dpi so that weird huge maximum scan area sizes
    // don't result in out of memory conditions when the user never even requests a large scan.
    double MAX_BLANK_PREVIEW_SIZE = 1000 * 1000;
    double MIN_BLANK_PREVIEW_SIZE = 200 * 200;
    if (dots_per_mm * dots_per_mm * width_mm * height_mm > MAX_BLANK_PREVIEW_SIZE) {
        dots_per_mm = std::sqrt(MAX_BLANK_PREVIEW_SIZE / (width_mm * height_mm));
    }
    if (dots_per_mm * dots_per_mm * width_mm * height_mm < MIN_BLANK_PREVIEW_SIZE) {
        dots_per_mm = std::sqrt(MIN_BLANK_PREVIEW_SIZE / (width_mm * height_mm));
    }

    dpi = dots_per_mm * 25.4;

    return PreviewConfig{width_mm, height_mm, static_cast<unsigned>(dpi)};
}

} // namespace

struct DocumentManager::Private {
    ScanEngine engine;
    QTimer engine_timer;

    bool all_documents_locked = false;

    std::string open_device_after_close;

    // Set when reopening device after a scan. Otherwise driver defaults will overwrite what's
    // stored on the document.
    bool ignore_next_option_values_change = false;

    std::vector<ScanDocument> documents;
    std::size_t curr_scan_document_index = 0;
    unsigned next_scan_id = 1;
};

DocumentManager::DocumentManager() :
    d_{std::make_unique<Private>()}
{
    connect(&d_->engine_timer, &QTimer::timeout, [this]() { periodic_engine_poll(); });
    connect(&d_->engine, &ScanEngine::start_polling, [this]() { d_->engine_timer.start(1); });
    connect(&d_->engine, &ScanEngine::stop_polling, [this]() { d_->engine_timer.stop(); });
    connect(&d_->engine, &ScanEngine::devices_refreshed, [this]() { devices_refreshed(); });
    connect(&d_->engine, &ScanEngine::options_changed, [this]() { options_changed(); });
    connect(&d_->engine, &ScanEngine::option_values_changed, [this]() { option_values_changed(); });
    connect(&d_->engine, &ScanEngine::device_opened, [this]() { device_opened(); });
    connect(&d_->engine, &ScanEngine::device_closed, [this]() { device_closed(); });
    connect(&d_->engine, &ScanEngine::image_updated, [this]() { image_updated(); });
    connect(&d_->engine, &ScanEngine::scan_finished, [this]() { scan_finished(); });
}

DocumentManager::~DocumentManager() = default;

void DocumentManager::refresh_devices()
{
    if (d_->all_documents_locked) {
        throw std::runtime_error("Can't refresh device when documents are locked");
    }
    d_->all_documents_locked = true;
    d_->engine.refresh_devices();
}

const std::vector<SaneDeviceInfo>& DocumentManager::available_devices()
{
    return d_->engine.current_devices();
}

void DocumentManager::select_device(unsigned doc_index, const std::string& name)
{
    auto& document = d_->documents.at(doc_index);
    if (document.locked || d_->all_documents_locked) {
        throw std::runtime_error("Can't select device when document is locked");
    }

    if (document.device.name == name) {
        return;
    }

    document.device = get_available_device_by_name(name);
    if (d_->curr_scan_document_index == doc_index) {
        if (d_->engine.is_device_opened()) {
            d_->engine.close_device();
            d_->open_device_after_close = name;
        } else {
            d_->engine.open_device(name);
        }
        d_->all_documents_locked = true;
        Q_EMIT document_locking_changed();
    }
}

void DocumentManager::start_scan(unsigned doc_index)
{
    auto& document = d_->documents.at(doc_index);
    auto& scan_document = curr_scan_document();

    if (doc_index != d_->curr_scan_document_index) {
        // We want to repeat scan of an existing document. The caller must ensure that the existing
        // scan was done for the same device.
        if (document.device.name != d_->engine.device_name()) {
            throw std::runtime_error("Can rescan document only on the same scanner");
        }
        scan_document.scan_option_descriptors = document.scan_option_descriptors;
        Q_EMIT document_option_descriptors_changed(d_->curr_scan_document_index);

        scan_document.scan_option_values = document.scan_option_values;
        Q_EMIT document_option_values_changed(d_->curr_scan_document_index);

        // Note that the preview is not touched as only the current scan document has it and it's
        // always for the current scanner.

        d_->engine.set_option_values(document.scan_option_values);
    }

    scan_document.locked = true;
    Q_EMIT document_locking_changed();

    scan_document.scan_progress = 0.0;
    Q_EMIT document_scan_progress_changed(d_->curr_scan_document_index);

    // We can't start scanning right away because option setup above may not have been completed
    // yet. These are done in-order, but any option reloads caused by setting the options will
    // start after this function completes, so requires additional synthronization to not
    // be issued when the scanning is active.
    d_->engine.call_when_idle([this]() { d_->engine.start_scan(); });
}

void DocumentManager::reopen_current_device()
{
    if (!d_->engine.is_device_opened()) {
        return;
    }

    d_->all_documents_locked = true;
    Q_EMIT document_locking_changed();
    d_->open_device_after_close = d_->engine.device_name();
    d_->engine.close_device();
}

const SaneDeviceInfo& DocumentManager::get_available_device_by_name(const std::string& name)
{
    const auto& devices = d_->engine.current_devices();
    auto it = std::find_if(devices.begin(), devices.end(),
                           [&](const auto& device) { return device.name == name; });
    if (it == devices.end()) {
        throw std::runtime_error("Could not find device with name " + name);
    }
    return *it;
}

ScanDocument& DocumentManager::curr_scan_document()
{
    return d_->documents.at(d_->curr_scan_document_index);
}

void DocumentManager::setup_empty_preview_image(ScanDocument& document,
                                                const std::optional<cv::Rect2d>& scan_bounds_mm)
{
    document.preview_scan_bounds = scan_bounds_mm;
    document.preview_config = setup_blank_preview_size(scan_bounds_mm);
    document.preview_image =
            cv::Mat(mm_to_inch(document.preview_config.height_mm) * document.preview_config.dpi,
                    mm_to_inch(document.preview_config.width_mm) * document.preview_config.dpi,
                    CV_8UC1, cv::Scalar(255, 255, 255));
}

void DocumentManager::clear_preview_image(ScanDocument& document)
{
    document.preview_scan_bounds.reset();
    document.preview_config = {};
    document.preview_image.reset();
}

void DocumentManager::set_document_option(unsigned doc_index, const std::string& name,
                                          const SaneOptionValue& value)
{
    auto& document = d_->documents.at(doc_index);
    if (document.locked || d_->all_documents_locked) {
        throw std::runtime_error("Can't change option when document is locked");
    }
    document.scan_option_values.insert_or_assign(name, value);
    d_->engine.set_option_value(name, value);
}

const ScanDocument& DocumentManager::document(unsigned index) const
{
    return d_->documents.at(index);
}

unsigned DocumentManager::curr_scan_document_index() const
{
    return d_->curr_scan_document_index;
}

bool DocumentManager::are_documents_globally_locked() const
{
    return d_->all_documents_locked;
}

void DocumentManager::periodic_engine_poll()
{
    try {
        d_->engine.perform_step();
    } catch (const std::exception& e) {
        // FIXME: we should show the error in the UI
        std::cerr << "SaneScan: Got error: " << e.what() << "\n";
        reopen_current_device();
    } catch (...) {
        // FIXME: we should show the error in the UI
        std::cerr << "SaneScan: Got error\n";
        reopen_current_device();
    }
}

void DocumentManager::devices_refreshed()
{
    d_->all_documents_locked = false;

    if (d_->documents.empty()) {
        d_->documents.emplace_back(d_->next_scan_id++);
        Q_EMIT new_document_added(0, false);
    }

    Q_EMIT available_devices_changed();
}

void DocumentManager::options_changed()
{
    auto& document = curr_scan_document();
    document.scan_option_descriptors = d_->engine.get_option_groups();
    Q_EMIT document_option_descriptors_changed(d_->curr_scan_document_index);

    auto scan_bounds = get_scan_size_from_options(document.scan_option_descriptors);
    if (document.preview_scan_bounds != scan_bounds) {
        setup_empty_preview_image(document, scan_bounds);
        Q_EMIT document_preview_image_changed(d_->curr_scan_document_index);
    }
}

void DocumentManager::option_values_changed()
{
    auto& document = curr_scan_document();
    if (d_->ignore_next_option_values_change) {
        d_->engine.set_option_values(document.scan_option_values);
        d_->ignore_next_option_values_change = false;
    } else {
        document.scan_option_values = d_->engine.get_option_values();
    }
    Q_EMIT document_option_values_changed(d_->curr_scan_document_index);
}

void DocumentManager::device_opened()
{
    d_->all_documents_locked = false;
    Q_EMIT document_locking_changed();

    auto device_name = d_->engine.device_name();
    for (std::size_t i = 0; i < d_->documents.size(); ++i) {
        auto& document = d_->documents[i];
        if (document.device.name != device_name) {
            clear_preview_image(document);
            Q_EMIT document_preview_image_changed(i);
        }
    }
}

void DocumentManager::device_closed()
{
    d_->all_documents_locked = true;
    Q_EMIT document_locking_changed();

    if (!d_->open_device_after_close.empty()) {
        std::string name;
        name.swap(d_->open_device_after_close);
        d_->engine.open_device(name);
    }
}

void DocumentManager::image_updated()
{
    auto& document = curr_scan_document();
    document.scanned_image = d_->engine.scan_image();
    Q_EMIT document_image_changed(d_->curr_scan_document_index);
}

void DocumentManager::scan_finished()
{
    {
        auto& document = curr_scan_document();
        document.scan_progress.reset();
        Q_EMIT document_scan_progress_changed(d_->curr_scan_document_index);
    }

    // Setup a new document that would serve as a template to repeat the current scan.
    {
        auto new_document_index = d_->documents.size();
        auto& new_document = d_->documents.emplace_back(d_->next_scan_id++);
        auto& document = curr_scan_document();
        new_document.device = document.device;
        std::swap(new_document.preview_config, document.preview_config);
        std::swap(new_document.preview_image, document.preview_image);
        std::swap(new_document.preview_scan_bounds, document.preview_scan_bounds);
        new_document.scan_option_descriptors = document.scan_option_descriptors;
        new_document.scan_option_values = document.scan_option_values;
        d_->curr_scan_document_index = new_document_index;
        Q_EMIT new_document_added(new_document_index, true);
    }

    // At least the genesys backend can't perform two scans back to back.
    d_->ignore_next_option_values_change = true;
    reopen_current_device();
}

} // namespace sanescan
