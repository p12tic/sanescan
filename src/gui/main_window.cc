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

#include "main_window.h"
#include "about_dialog.h"
#include "image_widget.h"
#include "qimage_utils.h"
#include "scan_settings_widget.h"
#include "scan_document.h"
#include "ui_main_window.h"
#include "pagelist/page_list_model.h"
#include "pagelist/page_list_view_delegate.h"
#include "../util/math.h"
#include <QtCore/QTimer>

#include <iostream>
#include <optional>
#include <stdexcept>

namespace sanescan {

namespace {

std::optional<QRectF>
    get_scan_size_from_options(const std::vector<SaneOptionGroupDestriptor>& options)
{
    auto tl_x_desc = find_option_descriptor(options, "tl-x");
    auto tl_y_desc = find_option_descriptor(options, "tl-y");
    auto br_x_desc = find_option_descriptor(options, "br-x");
    auto br_y_desc = find_option_descriptor(options, "br-y");

    if (!tl_x_desc || !tl_y_desc || !br_x_desc || !br_y_desc) {
        return {};
    }

    auto* tl_x_constraint = std::get_if<SaneConstraintFloatRange>(&tl_x_desc->constraint);
    auto* tl_y_constraint = std::get_if<SaneConstraintFloatRange>(&tl_y_desc->constraint);
    auto* br_x_constraint = std::get_if<SaneConstraintFloatRange>(&br_x_desc->constraint);
    auto* br_y_constraint = std::get_if<SaneConstraintFloatRange>(&br_y_desc->constraint);

    if (!tl_x_constraint || !tl_y_constraint || !br_x_constraint || !br_y_constraint) {
        return {};
    }

    QRectF rect = {tl_x_constraint->min, tl_y_constraint->min,
                   br_x_constraint->max - tl_x_constraint->min,
                   br_y_constraint->max - tl_y_constraint->min};
    return {rect.normalized()};
}

std::optional<QRectF>
    get_curr_scan_area_from_options(const std::map<std::string, SaneOptionValue>& options)
{
    auto get_value = [&](const std::string& name) -> std::optional<double>
    {
        if (options.count(name) == 0) {
            return {};
        }
        return options.at(name).as_double();
    };

    auto value_tl_x = get_value("tl-x");
    auto value_tl_y = get_value("tl-y");
    auto value_br_x = get_value("br-x");
    auto value_br_y = get_value("br-y");

    if (!value_tl_x.has_value() || !value_tl_y.has_value() ||
        !value_br_x.has_value() || !value_br_y.has_value()) {
        return {};
    }

    return QRectF{value_tl_x.value(), value_tl_y.value(),
                  value_br_x.value() - value_tl_x.value(), value_br_y.value() - value_tl_y.value()};
}

QRectF scan_space_to_scene_space(const QRectF& rect, double dpi)
{
    return QRectF{mm_to_inch(rect.left()) * dpi,
                  mm_to_inch(rect.top()) * dpi,
                  mm_to_inch(rect.right()) * dpi,
                  mm_to_inch(rect.bottom()) * dpi};
}

struct PreviewConfig {
    double width_mm = 0;
    double height_mm = 0;
    unsigned dpi = 0;
};

PreviewConfig get_default_preview_config()
{
    // Use A4 size by default. At the given dpi the blank image is relatively small at 163x233
    // pixels
    return PreviewConfig{210, 297, 20};
}

PreviewConfig setup_blank_preview_size(const std::optional<QRectF>& bounds_mm)
{
    if (!bounds_mm.has_value()) {
        return get_default_preview_config();
    }

    double width_mm = bounds_mm->width();
    double height_mm = bounds_mm->height();

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

struct MainWindow::Private {
    std::unique_ptr<Ui::MainWindow> ui;
    std::string open_device_after_close;
    ScanEngine engine;
    QTimer engine_timer;

    std::unique_ptr<PageListModel> page_list_model;
    std::uint64_t last_scan_id = 0;

    std::optional<QRectF> scan_bounds;
    QImage preview_image;
    PreviewConfig preview_config;

    std::vector<ScanDocument> documents;
    std::size_t curr_scan_document_index = 0;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    d_{std::make_unique<Private>()}
{
    d_->ui = std::make_unique<Ui::MainWindow>();
    d_->ui->setupUi(this);

    connect(d_->ui->action_about, &QAction::triggered, [this](){ present_about_dialog(); });

    connect(&d_->engine_timer, &QTimer::timeout, [this]()
    {
        try {
            d_->engine.perform_step();
        } catch (...) {
            // FIXME: we should show the error in the UI
            std::cerr << "SaneScan: Got error\n";
            if (d_->engine.is_device_opened()) {
                d_->ui->settings_widget->set_options_enabled(false);
                d_->open_device_after_close = d_->engine.device_name();
                d_->engine.close_device();
            }
        }
    });
    connect(&d_->engine, &ScanEngine::devices_refreshed, [this]() { devices_refreshed(); });
    connect(&d_->engine, &ScanEngine::start_polling, [this]() { d_->engine_timer.start(1); });
    connect(&d_->engine, &ScanEngine::stop_polling, [this]() { d_->engine_timer.stop(); });
    connect(&d_->engine, &ScanEngine::options_changed, [this]()
    {
        const auto& options = d_->engine.get_option_groups();

        auto scan_bounds = get_scan_size_from_options(options);
        if (d_->scan_bounds != scan_bounds) {
            d_->preview_image = QImage();
            d_->preview_config = {};
            d_->ui->image_area->set_selection_enabled(false);
            d_->scan_bounds = scan_bounds;
            setup_preview_image();
        }

        d_->ui->settings_widget->set_options(options);
    });

    connect(&d_->engine, &ScanEngine::option_values_changed, [this]()
    {
        d_->ui->settings_widget->set_option_values(d_->engine.get_option_values());
    });
    connect(&d_->engine, &ScanEngine::device_opened, [this]()
    {
        d_->ui->settings_widget->set_options_enabled(true);
    });
    connect(&d_->engine, &ScanEngine::device_closed, [this]()
    {
        d_->preview_image = QImage();
        d_->preview_config = {};
        d_->ui->image_area->set_selection_enabled(false);
        d_->scan_bounds.reset();

        if (!d_->open_device_after_close.empty()) {
            std::string name;
            name.swap(d_->open_device_after_close);
            d_->engine.open_device(name);
        }
    });
    connect(&d_->engine, &ScanEngine::image_updated, [this]()
    {
        d_->ui->image_area->set_image(qimage_from_cv_mat(d_->engine.scan_image()));
    });
    connect(&d_->engine, &ScanEngine::scan_finished, [this]()
    {
        d_->ui->settings_widget->set_options_enabled(true);
        scanning_finished();
    });

    connect(d_->ui->settings_widget, &ScanSettingsWidget::refresh_devices_clicked,
            [this]() { refresh_devices(); });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::device_selected,
            [this](const std::string& name) { select_device(name); });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::option_value_changed,
            [this](const auto& name, const auto& value)
    {
        maybe_update_selection_after_setting_change(name, value);
        d_->engine.set_option_value(name, value);
    });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::scan_started,
            [this]()
    {
        d_->preview_image = QImage();
        d_->preview_config = {};
        d_->ui->image_area->set_selection_enabled(false);
        d_->ui->settings_widget->set_options_enabled(false);
        d_->engine.start_scan();
    });

    d_->page_list_model = std::make_unique<PageListModel>(this);
    d_->ui->page_list->setModel(d_->page_list_model.get());
    d_->ui->page_list->setItemDelegate(new PageListViewDelegate(d_->ui->page_list));

    refresh_devices();
}

MainWindow::~MainWindow() = default;

void MainWindow::present_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::refresh_devices()
{
    d_->ui->stack_settings->setCurrentIndex(STACK_LOADING);
    d_->engine.refresh_devices();
}

void MainWindow::devices_refreshed()
{
    d_->ui->stack_settings->setCurrentIndex(STACK_SETTINGS);
    d_->ui->settings_widget->set_current_devices(d_->engine.current_devices());
}

void MainWindow::start_scanning()
{
    d_->engine.start_scan();
    d_->ui->stack_settings->setCurrentIndex(STACK_SCANNING);

    ScanDocument document;
    document.scan_id = d_->last_scan_id++;
    document.scanner_name = d_->engine.device_name();
    document.scan_option_descriptors = d_->engine.get_option_groups();
    document.scan_option_values = d_->engine.get_option_values();
    d_->curr_scan_document_index = d_->documents.size();
    d_->documents.push_back(document);
}

void MainWindow::scanning_finished()
{
    d_->ui->stack_settings->setCurrentIndex(STACK_SETTINGS);

    auto& scanned_document = d_->documents[d_->curr_scan_document_index];

    auto scanned_image = d_->engine.scan_image();
    scanned_document.source_image = scanned_image;
    d_->page_list_model->add_page(scanned_document.scan_id,
                                  qimage_from_cv_mat(scanned_image).copy());
}

void MainWindow::select_device(const std::string& name)
{
    d_->ui->settings_widget->set_options_enabled(false);
    // we are guaranteed by d_->ui->settings_widget that device_selected will not be emitted between
    // a previous emission to select_device and a call to device_opened.
    if (d_->engine.is_device_opened()) {
        d_->engine.close_device();
        d_->open_device_after_close = name;
    } else {
        d_->engine.open_device(name);
    }
}

void MainWindow::maybe_update_selection_after_setting_change(const std::string& name,
                                                             const SaneOptionValue& value)
{
    if (name != "tl-x" && name != "tl-y" && name != "br-x" && name != "br-y") {
        return;
    }

    if (!d_->ui->image_area->get_selection_enabled()) {
        return;
    }

    auto curr_scan_area_opt = get_curr_scan_area_from_options(d_->engine.get_option_values());
    if (!curr_scan_area_opt.has_value()) {
        return;
    }
    auto selection_rect = scan_space_to_scene_space(curr_scan_area_opt.value(),
                                                    d_->preview_config.dpi);

    auto value_as_double = value.as_double();
    if (!value_as_double.has_value()) {
        return;
    }

    auto scene_pos = mm_to_inch(value_as_double.value()) * d_->preview_config.dpi;

    if (name == "tl-x") {
        selection_rect.setLeft(scene_pos);
    }
    if (name == "tl-y") {
        selection_rect.setTop(scene_pos);
    }
    if (name == "br-x") {
        selection_rect.setRight(scene_pos);
    }
    if (name == "br-y") {
        selection_rect.setBottom(scene_pos);
    }
    d_->ui->image_area->set_selection(selection_rect.normalized());
}

void MainWindow::setup_preview_image()
{
    // TODO: only setup preview image when we don't have one for the current scanner
    d_->preview_config = setup_blank_preview_size(d_->scan_bounds);
    d_->preview_image = QImage(mm_to_inch(d_->preview_config.width_mm) * d_->preview_config.dpi,
                               mm_to_inch(d_->preview_config.height_mm) * d_->preview_config.dpi,
                               QImage::Format_Mono);
    d_->preview_image.fill(255);
    d_->ui->image_area->set_image(d_->preview_image);
    d_->ui->image_area->set_selection_enabled(d_->scan_bounds.has_value());
    connect(d_->ui->image_area, &ImageWidget::selection_changed, [this](std::optional<QRectF> rect)
    {
        double top = 0;
        double bottom = 0;
        double left = 0;
        double right = 0;
        if (rect.has_value()) {
            top = inch_to_mm(rect->top() / d_->preview_config.dpi);
            bottom = inch_to_mm(rect->bottom() / d_->preview_config.dpi);
            left = inch_to_mm(rect->left() / d_->preview_config.dpi);
            right = inch_to_mm(rect->right() / d_->preview_config.dpi);
        } else {
            if (!d_->scan_bounds.has_value()) {
                throw std::runtime_error("Scan bounds does not have value unexpectedly");
            }
            top = d_->scan_bounds->left();
            bottom = d_->scan_bounds->top();
            left = d_->scan_bounds->right();
            right = d_->scan_bounds->bottom();
        }

        d_->ui->settings_widget->set_option_value("tl-x", left);
        d_->ui->settings_widget->set_option_value("tl-y", top);
        d_->ui->settings_widget->set_option_value("br-x", right);
        d_->ui->settings_widget->set_option_value("br-y", bottom);

        // TODO: need to ensure that we set the values in correct order so that the scan window
        // is first widened, then shrunk. Otherwise we may create a situation with negative
        // size scan window and SANE driver will just ignore some of our settings.
        d_->engine.set_option_value("tl-x", left);
        d_->engine.set_option_value("tl-y", top);
        d_->engine.set_option_value("br-x", right);
        d_->engine.set_option_value("br-y", bottom);
    });
}

} // namespace sanescan
