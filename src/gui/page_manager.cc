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

#include "page_manager.h"
#include "scan_engine.h"
#include "lib/job_queue.h"
#include "lib/scan_area_utils.h"
#include "ocr/pdf_writer.h"
#include "util/math.h"

#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
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

const cv::Mat& image_to_save(const ScanPage& page, PageManager::SaveMode mode)
{
    if (mode == PageManager::SaveMode::RAW_SCAN) {
        if (!page.scanned_image.has_value()) {
            throw std::runtime_error("Can't save page without image");
        }
        return page.scanned_image.value();
    } else { // WITH_OCR
        if (!page.ocr_results.has_value()) {
            throw std::runtime_error("Can't save page without image");
        }
        return page.ocr_results->adjusted_image;
    }
}

} // namespace

struct PageManager::Private {
    ScanEngine engine;
    QTimer engine_timer;

    bool all_pages_locked = false;

    std::string open_device_after_close;

    // Set when reopening device after a scan. Otherwise driver defaults will overwrite what's
    // stored on the page.
    bool ignore_next_option_values_change = false;

    std::vector<ScanPage> pages;
    std::size_t curr_scan_page_index = 0;
    unsigned next_scan_id = 1;

    // Note that descroying PageManager will wait until all jobs submitted to the executor
    // complete.
    // FIXME: properly set the thread pool size
    JobQueue job_executor{4};
};

PageManager::PageManager() :
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

PageManager::~PageManager() = default;

void PageManager::refresh_devices()
{
    if (d_->all_pages_locked) {
        throw std::runtime_error("Can't refresh device when pages are locked");
    }
    d_->all_pages_locked = true;
    d_->engine.refresh_devices();
}

const std::vector<SaneDeviceInfo>& PageManager::available_devices()
{
    return d_->engine.current_devices();
}

void PageManager::select_device(unsigned page_index, const std::string& name)
{
    auto& page = d_->pages.at(page_index);
    if (page.locked || d_->all_pages_locked) {
        throw std::runtime_error("Can't select device when page is locked");
    }

    if (page.device.name == name) {
        return;
    }

    page.device = get_available_device_by_name(name);
    if (d_->curr_scan_page_index == page_index) {
        if (d_->engine.is_device_opened()) {
            d_->engine.close_device();
            d_->open_device_after_close = name;
        } else {
            d_->engine.open_device(name);
        }
        d_->all_pages_locked = true;
        Q_EMIT page_locking_changed();
    }
}

void PageManager::start_scan(unsigned page_index, ScanType type)
{
    auto& page = d_->pages.at(page_index);
    auto& scan_page = curr_scan_page();

    scan_page.scan_type = type;

    if (page_index != d_->curr_scan_page_index) {
        // We want to repeat scan of an existing page. The caller must ensure that the existing
        // scan was done for the same device.
        if (page.device.name != d_->engine.device_name()) {
            throw std::runtime_error("Can rescan page only on the same scanner");
        }
        scan_page.scan_option_descriptors = page.scan_option_descriptors;
        Q_EMIT page_option_descriptors_changed(d_->curr_scan_page_index);

        scan_page.scan_option_values = page.scan_option_values;
        Q_EMIT page_option_values_changed(d_->curr_scan_page_index);

        // Note that the preview image is not touched as only the current scan page has it and
        // it's always for the current scanner.

        if (type == ScanType::NORMAL) {
            d_->engine.set_option_values(page.scan_option_values);
        }
        // preview scans reset all values below
    }

    if (type == ScanType::PREVIEW)  {
        /*  For preview scan we override the bounds with maximum bounds and the resolution
            with the minimum resolution.

            scan_page.scan_option_descriptors corresponds to the descriptors for the
            particular option values, so we don't need to wait for updates to the option
            descriptors (changing e.g. scan source may change scan bounds) as we have that
            data already.
        */
        auto preview_scan_options = scan_page.scan_option_values;
        auto min_resolution = get_min_resolution(scan_page.scan_option_descriptors);
        if (min_resolution.has_value()) {
            preview_scan_options.insert_or_assign("resolution", min_resolution.value());
        }
        auto max_scan_size = get_scan_size_from_options(scan_page.scan_option_descriptors);
        if (max_scan_size.has_value()) {
            preview_scan_options.insert_or_assign("tl-x", max_scan_size.value().tl().x);
            preview_scan_options.insert_or_assign("tl-y", max_scan_size.value().tl().y);
            preview_scan_options.insert_or_assign("br-x", max_scan_size.value().br().x);
            preview_scan_options.insert_or_assign("br-y", max_scan_size.value().br().y);
        }
        d_->engine.set_option_values(preview_scan_options);
    }

    scan_page.locked = true;
    Q_EMIT page_locking_changed();

    scan_page.scan_progress = 0.0;
    Q_EMIT page_progress_changed(d_->curr_scan_page_index);

    // We can't start scanning right away because option setup above may not have been completed
    // yet. These are done in-order, but any option reloads caused by setting the options will
    // start after this function completes, so requires additional synthronization to not
    // be issued when the scanning is active.
    d_->engine.call_when_idle([this]() { d_->engine.start_scan(); });
}

void PageManager::on_ocr_complete(unsigned page_index)
{
    auto& page = d_->pages.at(page_index);

    bool updated_results = false;
    for (auto& job : page.ocr_jobs) {
        if (job->finished()) {
            if (job->job_id() == page.last_ocr_job_id) {
                page.ocr_results = std::move(job->results());
                page.ocr_progress.reset();
                updated_results = true;
            }
            job.reset();
        }
    }

    // remove all completed jobs
    auto it = std::remove_if(page.ocr_jobs.begin(), page.ocr_jobs.end(),
                             [](const auto& job) { return job.get() == nullptr; });
    page.ocr_jobs.erase(it, page.ocr_jobs.end());

    // We wait until the end of the function before notifying about results change to ensure that
    // the jobs array isn't changed while we're iterating over it.
    if (updated_results) {
        Q_EMIT page_progress_changed(page_index);
        Q_EMIT page_ocr_results_changed(page_index);
    }
}

void PageManager::reopen_current_device()
{
    if (!d_->engine.is_device_opened()) {
        return;
    }

    d_->all_pages_locked = true;
    Q_EMIT page_locking_changed();
    d_->open_device_after_close = d_->engine.device_name();
    d_->engine.close_device();
}

const SaneDeviceInfo& PageManager::get_available_device_by_name(const std::string& name)
{
    const auto& devices = d_->engine.current_devices();
    auto it = std::find_if(devices.begin(), devices.end(),
                           [&](const auto& device) { return device.name == name; });
    if (it == devices.end()) {
        throw std::runtime_error("Could not find device with name " + name);
    }
    return *it;
}

ScanPage& PageManager::curr_scan_page()
{
    return d_->pages.at(d_->curr_scan_page_index);
}

void PageManager::setup_empty_preview_image(ScanPage& page,
                                                const std::optional<cv::Rect2d>& scan_bounds_mm)
{
    page.preview_scan_bounds = scan_bounds_mm;
    page.preview_config = setup_blank_preview_size(scan_bounds_mm);
    page.preview_image =
            cv::Mat(mm_to_inch(page.preview_config.height_mm) * page.preview_config.dpi,
                    mm_to_inch(page.preview_config.width_mm) * page.preview_config.dpi,
                    CV_8UC1, cv::Scalar(255, 255, 255));
}

void PageManager::clear_preview_image(ScanPage& page)
{
    page.preview_scan_bounds.reset();
    page.preview_config = {};
    page.preview_image.reset();
}

void PageManager::perform_ocr(unsigned page_index, const OcrOptions& new_options)
{
    auto& page = d_->pages.at(page_index);
    page.ocr_jobs.push_back(std::make_unique<OcrJob>(page.scanned_image.value(),
                                                     new_options,
                                                     page.ocr_options,
                                                     page.ocr_results,
                                                      ++page.last_ocr_job_id,
                                                     [this, page_index]()
    {
        QMetaObject::invokeMethod(this, "on_ocr_complete", Qt::QueuedConnection,
                                  Q_ARG(unsigned, page_index));
    }));
    page.ocr_options = new_options;
    page.ocr_results.reset();
    page.ocr_progress = 0.0;
    d_->job_executor.submit(*(page.ocr_jobs.back().get()));

    Q_EMIT page_ocr_results_changed(page_index);
    Q_EMIT page_progress_changed(page_index);
}

void PageManager::set_page_option(unsigned page_index, const std::string& name,
                                  const SaneOptionValue& value)
{
    auto& page = d_->pages.at(page_index);
    if (page.locked || d_->all_pages_locked) {
        throw std::runtime_error("Can't change option when page is locked");
    }
    page.scan_option_values.insert_or_assign(name, value);
    d_->engine.set_option_value(name, value);
}

const ScanPage& PageManager::page(unsigned index) const
{
    return d_->pages.at(index);
}

unsigned PageManager::curr_scan_page_index() const
{
    return d_->curr_scan_page_index;
}

bool PageManager::are_pages_globally_locked() const
{
    return d_->all_pages_locked;
}

void PageManager::set_page_ocr_options(unsigned page_index, const OcrOptions& options)
{
    auto& page = d_->pages.at(page_index);
    if (page.ocr_options == options) {
        return;
    }
    if (!page.scanned_image.has_value()) {
        throw std::runtime_error("Document must have scanned image when setting options");
    }
    perform_ocr(page_index, options);
}

void PageManager::save_page(unsigned page_index, SaveMode mode, const std::string& path)
{
    std::filesystem::path p(path);
    auto is_pdf = p.extension().string() == ".pdf";

    auto& page = d_->pages.at(page_index);

    auto image = image_to_save(page, mode);
    if (is_pdf) {
        std::ofstream out_stream{p};
        PdfWriter writer{out_stream};
        writer.write_header();
        writer.write_page(image, {});
    } else {
        cv::imwrite(path, image);
    }
}

void PageManager::save_all_pages(SaveMode mode, const std::string& path)
{
    std::filesystem::path base_path(path);
    auto extension = base_path.extension().string();
    auto is_pdf = extension == ".pdf";

    // Note that we exclude the last page as it will always contain not yet finished scan.
    if (is_pdf) {
        std::ofstream out_stream{path};
        PdfWriter writer{out_stream};
        writer.write_header();
        for (std::size_t i = 0; i < d_->pages.size() - 1; ++i) {
            const auto& page = d_->pages.at(i);
            auto image = image_to_save(page, mode);

            if (mode == SaveMode::RAW_SCAN) {
                writer.write_page(image, {});
            } else {
                writer.write_page(image, page.ocr_results->adjusted_paragraphs);
            }
        }
    } else {
        auto base_dir = base_path.parent_path();
        auto base_stem = base_path.stem().string();

        for (std::size_t i = 0; i < d_->pages.size() - 1; ++i) {
            auto image = image_to_save(d_->pages.at(i), mode);
            auto image_path = base_dir / (base_stem + "-" + std::to_string(i + 1) + extension);
            cv::imwrite(image_path.string(), image);
        }
    }
}

void PageManager::periodic_engine_poll()
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

void PageManager::devices_refreshed()
{
    d_->all_pages_locked = false;

    if (d_->pages.empty()) {
        d_->pages.emplace_back(d_->next_scan_id++);
        Q_EMIT new_page_added(0, false);
    }

    Q_EMIT available_devices_changed();
}

void PageManager::options_changed()
{
    auto& page = curr_scan_page();
    if (page.scan_type == ScanType::PREVIEW) {
        return;
    }

    page.scan_option_descriptors = d_->engine.get_option_groups();
    Q_EMIT page_option_descriptors_changed(d_->curr_scan_page_index);

    auto scan_bounds = get_scan_size_from_options(page.scan_option_descriptors);
    if (page.preview_scan_bounds != scan_bounds) {
        setup_empty_preview_image(page, scan_bounds);
        Q_EMIT page_preview_image_changed(d_->curr_scan_page_index);
    }
}

void PageManager::option_values_changed()
{
    auto& page = curr_scan_page();
    if (page.scan_type == ScanType::PREVIEW) {
        return;
    }

    if (d_->ignore_next_option_values_change) {
        d_->engine.set_option_values(page.scan_option_values);
        d_->ignore_next_option_values_change = false;
    } else {
        page.scan_option_values = d_->engine.get_option_values();
    }
    Q_EMIT page_option_values_changed(d_->curr_scan_page_index);
}

void PageManager::device_opened()
{
    d_->all_pages_locked = false;
    Q_EMIT page_locking_changed();

    auto device_name = d_->engine.device_name();
    for (std::size_t i = 0; i < d_->pages.size(); ++i) {
        auto& page = d_->pages[i];
        if (page.device.name != device_name) {
            clear_preview_image(page);
            Q_EMIT page_preview_image_changed(i);
        }
    }
}

void PageManager::device_closed()
{
    d_->all_pages_locked = true;
    Q_EMIT page_locking_changed();

    if (!d_->open_device_after_close.empty()) {
        std::string name;
        name.swap(d_->open_device_after_close);
        d_->engine.open_device(name);
    }
}

void PageManager::image_updated()
{
    auto& page = curr_scan_page();
    if (page.scan_type == ScanType::NORMAL) {
        page.scanned_image = d_->engine.scan_image();
        Q_EMIT page_image_changed(d_->curr_scan_page_index);
    } else { // PREVIEW
        page.preview_image = d_->engine.scan_image();
        Q_EMIT page_preview_image_changed(d_->curr_scan_page_index);
    }
}

void PageManager::scan_finished()
{
    {
        auto& page = curr_scan_page();
        page.scan_progress.reset();
        Q_EMIT page_progress_changed(d_->curr_scan_page_index);
    }

    // Setup a new page that would serve as a template to repeat the current scan.
    if (curr_scan_page().scan_type == ScanType::NORMAL) {
        auto new_page_index = d_->pages.size();
        auto& new_page = d_->pages.emplace_back(d_->next_scan_id++);
        auto& page = curr_scan_page();
        auto old_page_index = d_->curr_scan_page_index;
        new_page.device = page.device;
        std::swap(new_page.preview_config, page.preview_config);
        std::swap(new_page.preview_image, page.preview_image);
        std::swap(new_page.preview_scan_bounds, page.preview_scan_bounds);
        new_page.scan_option_descriptors = page.scan_option_descriptors;
        new_page.scan_option_values = page.scan_option_values;
        d_->curr_scan_page_index = new_page_index;
        Q_EMIT new_page_added(new_page_index, true);
        perform_ocr(old_page_index, d_->pages.at(old_page_index).ocr_options);
    } else {
        auto& page = curr_scan_page();
        page.scan_type = ScanType::NORMAL;
        page.locked = false;
        Q_EMIT page_locking_changed();
    }

    // At least the genesys backend can't perform two scans back to back.
    d_->ignore_next_option_values_change = true;
    reopen_current_device();
}

} // namespace sanescan
