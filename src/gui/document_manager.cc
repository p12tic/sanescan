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
#include "lib/job_queue.h"
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

    // Note that descroying DocumentManager will wait until all jobs submitted to the executor
    // complete.
    // FIXME: properly set the thread pool size
    JobQueue job_executor{4};
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

    d_->job_executor.start();
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

void DocumentManager::start_scan(unsigned doc_index, ScanType type)
{
    auto& document = d_->documents.at(doc_index);
    auto& scan_document = curr_scan_document();

    scan_document.scan_type = type;

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

        // Note that the preview image is not touched as only the current scan document has it and
        // it's always for the current scanner.

        if (type == ScanType::NORMAL) {
            d_->engine.set_option_values(document.scan_option_values);
        }
        // preview scans reset all values below
    }

    if (type == ScanType::PREVIEW)  {
        /*  For preview scan we override the bounds with maximum bounds and the resolution
            with the minimum resolution.

            scan_document.scan_option_descriptors corresponds to the descriptors for the
            particular option values, so we don't need to wait for updates to the option
            descriptors (changing e.g. scan source may change scan bounds) as we have that
            data already.
        */
        auto preview_scan_options = scan_document.scan_option_values;
        auto min_resolution = get_min_resolution(scan_document.scan_option_descriptors);
        if (min_resolution.has_value()) {
            preview_scan_options.insert_or_assign("resolution", min_resolution.value());
        }
        auto max_scan_size = get_scan_size_from_options(scan_document.scan_option_descriptors);
        if (max_scan_size.has_value()) {
            preview_scan_options.insert_or_assign("tl-x", max_scan_size.value().tl().x);
            preview_scan_options.insert_or_assign("tl-y", max_scan_size.value().tl().y);
            preview_scan_options.insert_or_assign("br-x", max_scan_size.value().br().x);
            preview_scan_options.insert_or_assign("br-y", max_scan_size.value().br().y);
        }
        d_->engine.set_option_values(preview_scan_options);
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

void DocumentManager::on_ocr_complete(unsigned doc_index)
{
    auto& document = d_->documents.at(doc_index);

    bool updated_results = false;
    for (auto& job : document.ocr_jobs) {
        if (job->finished()) {
            if (job->job_id() == document.last_ocr_job_id) {
                document.ocr_results = std::move(job->results());
                document.ocr_progress.reset();
                updated_results = true;
            }
            job.reset();
        }
    }

    // remove all completed jobs
    auto it = std::remove_if(document.ocr_jobs.begin(), document.ocr_jobs.end(),
                             [](const auto& job) { return job.get() == nullptr; });
    document.ocr_jobs.erase(it, document.ocr_jobs.end());

    // We wait until the end of the function before notifying about results change to ensure that
    // the jobs array isn't changed while we're iterating over it.
    if (updated_results) {
        Q_EMIT document_ocr_results_changed(doc_index);
    }
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

void DocumentManager::perform_ocr(unsigned doc_index)
{
    auto& document = d_->documents.at(doc_index);
    document.ocr_results.reset();
    document.ocr_progress = 0.0;
    document.ocr_jobs.push_back(std::make_unique<OcrJob>(document.scanned_image.value(),
                                                         document.ocr_options,
                                                          ++document.last_ocr_job_id,
                                                         [this, doc_index]()
    {
        QMetaObject::invokeMethod(this, "on_ocr_complete", Qt::QueuedConnection,
                                  Q_ARG(unsigned, doc_index));
    }));
    d_->job_executor.submit(*(document.ocr_jobs.back().get()));

    Q_EMIT document_ocr_results_changed(doc_index);
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

void DocumentManager::set_document_ocr_options(unsigned doc_index, const OcrOptions& options)
{
    auto& document = d_->documents.at(doc_index);
    if (document.ocr_options == options) {
        return;
    }
    if (!document.scanned_image.has_value()) {
        throw std::runtime_error("Document must have scanned image when setting options");
    }

    document.ocr_options = options;
    perform_ocr(doc_index);
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
    if (document.scan_type == ScanType::PREVIEW) {
        return;
    }

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
    if (document.scan_type == ScanType::PREVIEW) {
        return;
    }

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
    if (document.scan_type == ScanType::NORMAL) {
        document.scanned_image = d_->engine.scan_image();
        Q_EMIT document_image_changed(d_->curr_scan_document_index);
    } else { // PREVIEW
        document.preview_image = d_->engine.scan_image();
        Q_EMIT document_preview_image_changed(d_->curr_scan_document_index);
    }
}

void DocumentManager::scan_finished()
{
    {
        auto& document = curr_scan_document();
        document.scan_progress.reset();
        Q_EMIT document_scan_progress_changed(d_->curr_scan_document_index);
    }

    // Setup a new document that would serve as a template to repeat the current scan.
    if (curr_scan_document().scan_type == ScanType::NORMAL) {
        auto new_document_index = d_->documents.size();
        auto& new_document = d_->documents.emplace_back(d_->next_scan_id++);
        auto& document = curr_scan_document();
        auto old_document_index = d_->curr_scan_document_index;
        new_document.device = document.device;
        std::swap(new_document.preview_config, document.preview_config);
        std::swap(new_document.preview_image, document.preview_image);
        std::swap(new_document.preview_scan_bounds, document.preview_scan_bounds);
        new_document.scan_option_descriptors = document.scan_option_descriptors;
        new_document.scan_option_values = document.scan_option_values;
        d_->curr_scan_document_index = new_document_index;
        Q_EMIT new_document_added(new_document_index, true);
        perform_ocr(old_document_index);
    } else {
        auto& document = curr_scan_document();
        document.scan_type = ScanType::NORMAL;
        document.locked = false;
        Q_EMIT document_locking_changed();
    }

    // At least the genesys backend can't perform two scans back to back.
    d_->ignore_next_option_values_change = true;
    reopen_current_device();
}

} // namespace sanescan
