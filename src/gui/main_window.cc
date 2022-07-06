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
#include "page_manager.h"
#include "image_widget.h"
#include "image_widget_ocr_results_manager.h"
#include "qimage_utils.h"
#include "scan_settings_widget.h"
#include "scan_page.h"
#include "ui_main_window.h"
#include "pagelist/page_list_model.h"
#include "pagelist/page_list_view_delegate.h"
#include "../util/math.h"
#include "../lib/scan_area_utils.h"

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace sanescan {

namespace {

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
    PageManager manager;

    std::unique_ptr<ImageWidgetOcrResultsManager> ocr_results_manager;

    std::unique_ptr<PageListModel> page_list_model;

    unsigned active_page_index = 0;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    d_{std::make_unique<Private>()}
{
    d_->ui = std::make_unique<Ui::MainWindow>();
    d_->ui->setupUi(this);

    d_->ui->stack_settings->setCurrentIndex(STACK_LOADING);
    d_->ui->tabs->setCurrentIndex(TAB_SCANNING);
    d_->ui->tabs->setTabEnabled(TAB_OCR, false);
    d_->ui->label_blurry_warning->setVisible(false);

    d_->ocr_results_manager =
            std::make_unique<ImageWidgetOcrResultsManager>(d_->ui->image_area->scene());

    connect(d_->ui->action_about, &QAction::triggered, [this](){ present_about_dialog(); });
    connect(d_->ui->action_save_current_image, &QAction::triggered,
            [this](){ save_current_page(); });
    connect(d_->ui->action_save_all_pages, &QAction::triggered,
            [this](){ save_all_pages(); });
    connect(d_->ui->action_save_all_pages_with_ocr, &QAction::triggered,
            [this](){ save_all_pages_with_ocr(); });

    connect(&d_->manager, &PageManager::available_devices_changed, [this]()
    {
        d_->ui->stack_settings->setCurrentIndex(STACK_SETTINGS);
        auto& page = d_->manager.page(d_->active_page_index);
        if (!page.locked) {
            // If page is locked then we don't care what are the available devices. What's
            // important is what the page was scanned with.
            d_->ui->settings_widget->set_current_devices(d_->manager.available_devices());
        }
    });

    connect(&d_->manager, &PageManager::page_option_descriptors_changed,
            [this](unsigned page_index)
    {
        if (d_->active_page_index != page_index) {
            return;
        }

        auto& page = d_->manager.page(page_index);
        d_->ui->settings_widget->set_options(page.scan_option_descriptors);
    });

    connect(&d_->manager, &PageManager::page_option_values_changed,
            [this](unsigned page_index)
    {
        if (d_->active_page_index != page_index) {
            return;
        }

        auto& page = d_->manager.page(page_index);
        d_->ui->settings_widget->set_option_values(page.scan_option_values);
    });

    connect(&d_->manager, &PageManager::page_locking_changed, [this]()
    {
        auto& page = d_->manager.page(d_->active_page_index);
        bool enabled = !(page.locked || d_->manager.are_pages_globally_locked());
        d_->ui->settings_widget->set_options_enabled(enabled);
        d_->ui->image_area->set_selection_enabled(enabled);
        if (enabled) {
            update_selection_to_settings();
        }
    });

    connect(&d_->manager, &PageManager::page_image_changed, [this](unsigned page_index)
    {
        if (d_->active_page_index != page_index) {
            return;
        }
        auto& page = d_->manager.page(page_index);
        if (!page.scanned_image.has_value()) {
            throw std::runtime_error("Document image changed, but it is not set");
        }
        d_->ui->image_area->set_image(qimage_from_cv_mat(page.scanned_image.value()));

        // FIXME: should probably change thumbnails even for inactive images
        d_->page_list_model->set_image(page.scan_id, get_page_thumbnail(page));
    });

    connect(&d_->manager, &PageManager::page_preview_image_changed,
            [this](unsigned page_index)
    {
        auto& page = d_->manager.page(page_index);
        if (d_->active_page_index == page_index) {
            if (!page.scanned_image.has_value() &&
                page.preview_image.has_value())
            {
                d_->ui->image_area->set_image(qimage_from_cv_mat(page.preview_image.value()));
            }
        }

        d_->page_list_model->set_image(page.scan_id, get_page_thumbnail(page));
        update_selection_to_settings();
    });

    connect(&d_->manager, &PageManager::page_progress_changed,
            [this](unsigned page_index)
    {
        if (d_->active_page_index != page_index) {
            return;
        }

        auto& page = d_->manager.page(page_index);
        d_->ui->label_ocr_progress->setVisible(page.ocr_progress.has_value());
        d_->ui->label_blurry_warning->setVisible(page.ocr_results.has_value() &&
                                                 page.ocr_results->blurred_words.size() > 2);
    });

    connect(&d_->manager, &PageManager::new_page_added,
            [this](unsigned page_index, bool after_scan)
    {
        d_->ui->action_save_current_image->setEnabled(true);
        d_->ui->action_save_all_pages->setEnabled(true);
        auto& page = d_->manager.page(page_index);
        d_->page_list_model->add_page(page.scan_id, get_page_thumbnail(page));
        if (after_scan) {
            switch_to_page(page_index);
        }
    });
    connect(&d_->manager, &PageManager::page_ocr_results_changed,
            [this](unsigned page_index)
    {
        d_->ui->action_save_all_pages_with_ocr->setEnabled(true);

        if (d_->active_page_index != page_index) {
            return;
        }

        auto& page = d_->manager.page(page_index);
        d_->ui->image_area->set_image(get_page_image(page));
        update_ocr_results_manager();
    });

    connect(d_->ui->settings_widget, &ScanSettingsWidget::refresh_devices_clicked,
            [this]() { d_->manager.refresh_devices(); });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::device_selected,
            [this](const std::string& name)
    {
        d_->manager.select_device(d_->active_page_index, name);
    });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::option_value_changed,
            [this](const auto& name, const auto& value)
    {
        d_->manager.set_page_option(d_->active_page_index, name, value);
        if (name == "tl-x" || name == "tl-y" || name == "br-x" || name == "br-y") {
            update_selection_to_settings();
        }
    });
    connect(d_->ui->settings_widget, &ScanSettingsWidget::scan_started,
            [this](ScanType type) { start_scanning(type); });

    connect(d_->ui->image_area, &ImageWidget::selection_changed,
            [this](const auto& rect) { image_area_selection_changed(rect); });

    connect(d_->ui->tabs, &QTabWidget::currentChanged,
            [this](int)
    {
        auto& page = d_->manager.page(d_->active_page_index);
        d_->ui->image_area->set_image(get_page_image(page));
        update_ocr_results_manager();
    });

    connect(d_->ui->ocr_settings, &OcrSettingsWidget::options_changed,
            [this](const OcrOptions& options)
    {
        d_->manager.set_page_ocr_options(d_->active_page_index, options);
    });
    connect(d_->ui->ocr_settings, &OcrSettingsWidget::should_highlight_text_changed,
            [this](bool should_highlight)
    {
        d_->ocr_results_manager->set_show_bounding_boxes(should_highlight);
        d_->ocr_results_manager->set_show_text(should_highlight);
        d_->ocr_results_manager->set_show_text_white_background(should_highlight);
        d_->ocr_results_manager->set_show_blur_warning_boxes(should_highlight);
    });

    d_->page_list_model = std::make_unique<PageListModel>(this);
    d_->ui->page_list->setModel(d_->page_list_model.get());
    d_->ui->page_list->setItemDelegate(new PageListViewDelegate(d_->ui->page_list));

    connect(d_->ui->page_list->selectionModel(), &QItemSelectionModel::selectionChanged,
            [this](const QItemSelection& selected, const QItemSelection& deselected)
    {
        switch_to_page(selected.front().top());
    });

    d_->manager.refresh_devices();
}

MainWindow::~MainWindow() = default;

void MainWindow::present_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::start_scanning(ScanType type)
{
    d_->manager.start_scan(d_->active_page_index, type);
    if (d_->manager.curr_scan_page_index() != d_->active_page_index) {
        switch_to_page(d_->manager.curr_scan_page_index());
    }
}

QImage MainWindow::get_page_thumbnail(const ScanPage& page)
{
    // FIXME: resize images to smaller size to avoid wasting excessive amounts of memory
    if (page.scanned_image.has_value()) {
        return qimage_from_cv_mat(page.scanned_image.value()).copy();
    }
    if (page.preview_image.has_value()) {
        return qimage_from_cv_mat(page.preview_image.value()).copy();
    }
    // TODO: add a placeholder image here
    auto image = QImage(100, 100, QImage::Format_Mono);
    image.fill(255);
    return image;
}

QImage MainWindow::get_page_image(const ScanPage& page)
{
    if (d_->ui->tabs->currentIndex() == TAB_OCR && page.ocr_results.has_value()) {
        // FIXME: store a reference somewhere so the copy is not needed
        return qimage_from_cv_mat(page.ocr_results->adjusted_image).copy();
    }
    if (page.scanned_image.has_value()) {
        return qimage_from_cv_mat(page.scanned_image.value()).copy();
    }
    if (page.preview_image.has_value()) {
        return qimage_from_cv_mat(page.preview_image.value()).copy();
    }
    throw std::runtime_error("Could not get page image. This should never happen");
}

void MainWindow::switch_to_page(unsigned page_index)
{
    d_->active_page_index = page_index;

    auto& page = d_->manager.page(page_index);
    bool enabled = !(page.locked || d_->manager.are_pages_globally_locked());

    d_->ui->page_list->selectionModel()->select(d_->page_list_model->index(page_index),
                                                QItemSelectionModel::ClearAndSelect);
    d_->ui->settings_widget->set_options(page.scan_option_descriptors);
    d_->ui->settings_widget->set_option_values(page.scan_option_values);
    d_->ui->settings_widget->set_options_enabled(enabled);
    d_->ui->image_area->set_selection_enabled(enabled);

    if (page.scanned_image.has_value()) {
        d_->ui->tabs->setTabEnabled(TAB_OCR, true);
        update_ocr_tab_to_settings();
    } else {
        d_->ui->tabs->setTabEnabled(TAB_OCR, false);
        d_->ui->tabs->setCurrentIndex(TAB_SCANNING);
    }
    d_->ui->image_area->set_image(get_page_image(page));
    d_->ui->label_ocr_progress->setVisible(page.ocr_progress.has_value());
    d_->ui->label_blurry_warning->setVisible(page.ocr_results.has_value() &&
                                             page.ocr_results->blurred_words.size() > 2);

    update_ocr_results_manager();
    update_selection_to_settings();
}

void MainWindow::update_selection_to_settings()
{
    auto& page = d_->manager.page(d_->active_page_index);
    auto max_scan_area = get_scan_size_from_options(page.scan_option_descriptors);
    auto curr_scan_area = get_curr_scan_area_from_options(page.scan_option_values);

    if (!max_scan_area.has_value() || !curr_scan_area.has_value()) {
        d_->ui->image_area->set_selection({});
        return;
    }

    if (rect_almost_equal(curr_scan_area.value(), max_scan_area.value(), 0.1)) {
        d_->ui->image_area->set_selection({});
        return;
    }

    auto selection_rect = scan_space_to_scene_space(qrectf_from_cv_rect2d(curr_scan_area.value()),
                                                    page.preview_config.dpi);
    d_->ui->image_area->set_selection(selection_rect.normalized());
}

void MainWindow::image_area_selection_changed(const std::optional<QRectF>& rect)
{
    auto& page = d_->manager.page(d_->active_page_index);

    double top = 0;
    double bottom = 0;
    double left = 0;
    double right = 0;
    if (rect.has_value()) {
        top = inch_to_mm(rect->top() / page.preview_config.dpi);
        bottom = inch_to_mm(rect->bottom() / page.preview_config.dpi);
        left = inch_to_mm(rect->left() / page.preview_config.dpi);
        right = inch_to_mm(rect->right() / page.preview_config.dpi);
    } else {
        if (!page.preview_scan_bounds.has_value()) {
            throw std::runtime_error("Scan bounds does not have value unexpectedly");
        }
        top = page.preview_scan_bounds->tl().x;
        bottom = page.preview_scan_bounds->tl().y;
        left = page.preview_scan_bounds->br().x;
        right = page.preview_scan_bounds->br().y;
    }

    d_->ui->settings_widget->set_option_value("tl-x", left);
    d_->ui->settings_widget->set_option_value("tl-y", top);
    d_->ui->settings_widget->set_option_value("br-x", right);
    d_->ui->settings_widget->set_option_value("br-y", bottom);

    // TODO: need to ensure that we set the values in correct order so that the scan window
    // is first widened, then shrunk. Otherwise we may create a situation with negative
    // size scan window and SANE driver will just ignore some of our settings.
    d_->manager.set_page_option(d_->active_page_index, "tl-x", left);
    d_->manager.set_page_option(d_->active_page_index, "tl-y", top);
    d_->manager.set_page_option(d_->active_page_index, "br-x", right);
    d_->manager.set_page_option(d_->active_page_index, "br-y", bottom);
}

void MainWindow::update_ocr_tab_to_settings()
{
    auto& page = d_->manager.page(d_->active_page_index);

    d_->ui->ocr_settings->set_options(page.ocr_options);
}

void MainWindow::update_ocr_results_manager()
{
    auto& page = d_->manager.page(d_->active_page_index);

    if (d_->ui->tabs->currentIndex() == TAB_OCR && page.ocr_results) {
        bool should_highlight = d_->ui->ocr_settings->should_highlight_text();
        d_->ocr_results_manager->set_show_bounding_boxes(should_highlight);
        d_->ocr_results_manager->set_show_text(should_highlight);
        d_->ocr_results_manager->set_show_text_white_background(should_highlight);
        d_->ocr_results_manager->set_show_blur_warning_boxes(should_highlight);

        d_->ocr_results_manager->setup(page.ocr_results->adjusted_paragraphs,
                                       page.ocr_results->blurred_words);
    } else {
        d_->ocr_results_manager->clear();
    }
}

void MainWindow::save_all_pages()
{
    auto path = QFileDialog::getSaveFileName(this, tr("Save all pages"), "",
                                             tr("Image Files (*.jpg *.png *.tiff *.pdf)"));

    if (!warn_if_is_unsupported_save_path(path.toStdString())) {
        return;
    }

    d_->manager.save_all_pages(PageManager::SaveMode::RAW_SCAN, path.toStdString());
}

void MainWindow::save_all_pages_with_ocr()
{
    auto path = QFileDialog::getSaveFileName(this, tr("Save all pages with OCR"), "",
                                             tr("Image Files (*.jpg *.png *.tiff *.pdf)"));

    if (!warn_if_is_unsupported_save_path(path.toStdString())) {
        return;
    }

    d_->manager.save_all_pages(PageManager::SaveMode::WITH_OCR, path.toStdString());
}

void MainWindow::save_current_page()
{
    auto path = QFileDialog::getSaveFileName(this, tr("Save current page"), "",
                                             tr("Image Files (*.jpg *.png *.tiff *.pdf)"));
    auto save_mode = d_->ui->tabs->currentIndex() == TAB_OCR
            ? PageManager::SaveMode::WITH_OCR
            : PageManager::SaveMode::RAW_SCAN;

    if (!warn_if_is_unsupported_save_path(path.toStdString())) {
        return;
    }

    d_->manager.save_page(d_->active_page_index, save_mode, path.toStdString());
}

bool MainWindow::warn_if_is_unsupported_save_path(const std::string& path)
{
    if (!is_supported_save_path(path)) {
        QMessageBox msg_box;
        msg_box.setText("The path extension is for unsupported image format\n"
                        "Supported formats: *.jpg *.png *.tiff *.pdf");
        msg_box.exec();
        return false;
    }
    return true;
}

bool MainWindow::is_supported_save_path(const std::string& path)
{
    std::filesystem::path base_path(path);
    auto extension = base_path.extension().string();

    std::vector<std::string> supported = {
        ".jpg",
        ".png",
        ".tiff",
        ".pdf",
    };

    return std::find(supported.begin(), supported.end(), extension) != supported.end();
}

} // namespace sanescan
