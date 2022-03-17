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
#include "document_manager.h"
#include "image_widget.h"
#include "qimage_utils.h"
#include "scan_settings_widget.h"
#include "scan_document.h"
#include "ui_main_window.h"
#include "pagelist/page_list_model.h"
#include "pagelist/page_list_view_delegate.h"
#include "../util/math.h"

#include <iostream>
#include <optional>
#include <stdexcept>

namespace sanescan {

namespace {


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

} // namespace

struct MainWindow::Private {
    std::unique_ptr<Ui::MainWindow> ui;
    DocumentManager manager;

    std::unique_ptr<PageListModel> page_list_model;

    unsigned active_document_index = 0;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    d_{std::make_unique<Private>()}
{
    d_->ui = std::make_unique<Ui::MainWindow>();
    d_->ui->setupUi(this);

    d_->ui->stack_settings->setCurrentIndex(STACK_LOADING);

    connect(d_->ui->action_about, &QAction::triggered, [this](){ present_about_dialog(); });
    connect(&d_->manager, &DocumentManager::available_devices_changed, [this]()
    {
        d_->ui->stack_settings->setCurrentIndex(STACK_SETTINGS);
        auto& document = d_->manager.document(d_->active_document_index);
        if (!document.locked) {
            // If document is locked then we don't care what are the available devices. What's
            // important is what the document was scanned with.
            d_->ui->settings_widget->set_current_devices(d_->manager.available_devices());
        }
    });

    connect(&d_->manager, &DocumentManager::document_option_descriptors_changed,
            [this](unsigned doc_index)
    {
        if (d_->active_document_index != doc_index) {
            return;
        }

        auto& document = d_->manager.document(doc_index);
        d_->ui->settings_widget->set_options(document.scan_option_descriptors);
    });

    connect(&d_->manager, &DocumentManager::document_option_values_changed,
            [this](unsigned doc_index)
    {
        if (d_->active_document_index != doc_index) {
            return;
        }

        auto& document = d_->manager.document(doc_index);
        d_->ui->settings_widget->set_option_values(document.scan_option_values);
    });

    connect(&d_->manager, &DocumentManager::document_locking_changed, [this]()
    {
        auto& document = d_->manager.document(d_->active_document_index);
        bool enabled = !(document.locked || d_->manager.are_documents_globally_locked());
        d_->ui->settings_widget->set_options_enabled(enabled);
        d_->ui->image_area->set_selection_enabled(enabled);
    });

    connect(&d_->manager, &DocumentManager::document_image_changed, [this](unsigned doc_index)
    {
        if (d_->active_document_index != doc_index) {
            return;
        }
        auto& document = d_->manager.document(doc_index);
        if (!document.scanned_image.has_value()) {
            throw std::runtime_error("Document image changed, but it is not set");
        }
        d_->ui->image_area->set_image(qimage_from_cv_mat(document.scanned_image.value()));

        // FIXME: should probably change thumbnails even for inactive images
        d_->page_list_model->set_image(document.scan_id, get_document_thumbnail(document));
    });

    connect(&d_->manager, &DocumentManager::document_preview_image_changed,
            [this](unsigned doc_index)
    {
        auto& document = d_->manager.document(doc_index);
        if (d_->active_document_index == doc_index) {
            if (!document.scanned_image.has_value() &&
                document.preview_image.has_value())
            {
                d_->ui->image_area->set_image(qimage_from_cv_mat(document.preview_image.value()));
            }
        }

        d_->page_list_model->set_image(document.scan_id, get_document_thumbnail(document));
    });

    connect(&d_->manager, &DocumentManager::document_scan_progress_changed,
            [this](unsigned doc_index)
    {
        if (d_->active_document_index != doc_index) {
            return;
        }

        auto& document = d_->manager.document(doc_index);
        if (document.scan_progress.has_value()) {
            d_->ui->stack_settings->setCurrentIndex(STACK_SCANNING);
            int value_percent = static_cast<int>(document.scan_progress.value() / 100);
            d_->ui->progress_scanning->setValue(value_percent);
        } else {
            d_->ui->stack_settings->setCurrentIndex(STACK_SETTINGS);
        }
    });

    connect(&d_->manager, &DocumentManager::new_document_added,
            [this](unsigned doc_index, bool after_scan)
    {
        auto& document = d_->manager.document(doc_index);
        d_->page_list_model->add_page(document.scan_id, get_document_thumbnail(document));
        if (after_scan) {
            switch_to_document(doc_index);
        }
    });

    connect(d_->ui->settings_widget, &ScanSettingsWidget::refresh_devices_clicked,
            [this]() { d_->manager.refresh_devices(); });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::device_selected,
            [this](const std::string& name)
    {
        d_->manager.select_device(d_->active_document_index, name);
    });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::option_value_changed,
            [this](const auto& name, const auto& value)
    {
        maybe_update_selection_after_setting_change(name, value);
        d_->manager.set_document_option(d_->active_document_index, name, value);
    });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::scan_started,
            [this]() { start_scanning(); });

    connect(d_->ui->image_area, &ImageWidget::selection_changed,
            [this](const auto& rect) { image_area_selection_changed(rect); });

    d_->page_list_model = std::make_unique<PageListModel>(this);
    d_->ui->page_list->setModel(d_->page_list_model.get());
    d_->ui->page_list->setItemDelegate(new PageListViewDelegate(d_->ui->page_list));

    d_->manager.refresh_devices();
}

MainWindow::~MainWindow() = default;

void MainWindow::present_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::start_scanning()
{
    d_->manager.start_scan(d_->active_document_index);
    if (d_->manager.curr_scan_document_index() != d_->active_document_index) {
        switch_to_document(d_->manager.curr_scan_document_index());
    }
}

QImage MainWindow::get_document_thumbnail(const ScanDocument& document)
{
    // FIXME: resize images to smaller size to avoid wasting excessive amounts of memory
    if (document.scanned_image.has_value()) {
        return qimage_from_cv_mat(document.scanned_image.value()).copy();
    }
    if (document.preview_image.has_value()) {
        return qimage_from_cv_mat(document.preview_image.value()).copy();
    }
    // TODO: add a placeholder image here
    auto image = QImage(100, 100, QImage::Format_Mono);
    image.fill(255);
    return image;
}

void MainWindow::switch_to_document(unsigned doc_index)
{
    d_->active_document_index = doc_index;

    auto& document = d_->manager.document(doc_index);
    bool enabled = !(document.locked || d_->manager.are_documents_globally_locked());

    d_->ui->page_list->selectionModel()->select(d_->page_list_model->index(doc_index),
                                                QItemSelectionModel::ClearAndSelect);
    d_->ui->settings_widget->set_options(document.scan_option_descriptors);
    d_->ui->settings_widget->set_option_values(document.scan_option_values);
    d_->ui->settings_widget->set_options_enabled(enabled);
    d_->ui->image_area->set_selection_enabled(enabled);

    if (document.scanned_image.has_value()) {
        d_->ui->image_area->set_image(qimage_from_cv_mat(document.scanned_image.value()));
    } else if (document.preview_image.has_value()) {
        d_->ui->image_area->set_image(qimage_from_cv_mat(document.preview_image.value()));
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

    auto& document = d_->manager.document(d_->active_document_index);

    auto curr_scan_area_opt = get_curr_scan_area_from_options(document.scan_option_values);
    if (!curr_scan_area_opt.has_value()) {
        return;
    }

    auto selection_rect = scan_space_to_scene_space(curr_scan_area_opt.value(),
                                                    document.preview_config.dpi);

    auto value_as_double = value.as_double();
    if (!value_as_double.has_value()) {
        return;
    }

    auto scene_pos = mm_to_inch(value_as_double.value()) * document.preview_config.dpi;

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

void MainWindow::image_area_selection_changed(const std::optional<QRectF>& rect)
{
    auto& document = d_->manager.document(d_->active_document_index);

    double top = 0;
    double bottom = 0;
    double left = 0;
    double right = 0;
    if (rect.has_value()) {
        top = inch_to_mm(rect->top() / document.preview_config.dpi);
        bottom = inch_to_mm(rect->bottom() / document.preview_config.dpi);
        left = inch_to_mm(rect->left() / document.preview_config.dpi);
        right = inch_to_mm(rect->right() / document.preview_config.dpi);
    } else {
        if (!document.preview_scan_bounds.has_value()) {
            throw std::runtime_error("Scan bounds does not have value unexpectedly");
        }
        top = document.preview_scan_bounds->left();
        bottom = document.preview_scan_bounds->top();
        left = document.preview_scan_bounds->right();
        right = document.preview_scan_bounds->bottom();
    }

    d_->ui->settings_widget->set_option_value("tl-x", left);
    d_->ui->settings_widget->set_option_value("tl-y", top);
    d_->ui->settings_widget->set_option_value("br-x", right);
    d_->ui->settings_widget->set_option_value("br-y", bottom);

    // TODO: need to ensure that we set the values in correct order so that the scan window
    // is first widened, then shrunk. Otherwise we may create a situation with negative
    // size scan window and SANE driver will just ignore some of our settings.
    d_->manager.set_document_option(d_->active_document_index, "tl-x", left);
    d_->manager.set_document_option(d_->active_document_index, "tl-y", top);
    d_->manager.set_document_option(d_->active_document_index, "br-x", right);
    d_->manager.set_document_option(d_->active_document_index, "br-y", bottom);
}

} // namespace sanescan
