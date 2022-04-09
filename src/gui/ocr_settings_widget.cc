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

#include "ocr_settings_widget.h"
#include "ui_ocr_settings_widget.h"

namespace sanescan {

struct OcrSettingsWidget::Private {
    std::unique_ptr<Ui::OcrSettingsWidget> ui;
    bool is_updating_from_code = false;
};

OcrSettingsWidget::OcrSettingsWidget(QWidget *parent) :
    QFrame(parent),
    d_{std::make_unique<Private>()}
{
    d_->ui = std::make_unique<Ui::OcrSettingsWidget>();
    d_->ui->setupUi(this);

    connect(d_->ui->checkbox_orientation_detect, &QCheckBox::toggled,
            [this](bool){ options_changed_impl(); });
    connect(d_->ui->spinbox_orientation_fraction, &QSpinBox::textChanged,
            [this](const QString&){ options_changed_impl(); });
    connect(d_->ui->spinbox_orientation_diff, &QSpinBox::textChanged,
            [this](const QString&){ options_changed_impl(); });
    connect(d_->ui->checkbox_rotate_detect, &QCheckBox::toggled,
            [this](bool){ options_changed_impl(); });
    connect(d_->ui->spinbox_rotate_fraction, &QSpinBox::textChanged,
            [this](const QString&){ options_changed_impl(); });
    connect(d_->ui->spinbox_rotate_diff, &QSpinBox::textChanged,
            [this](const QString&){ options_changed_impl(); });
    connect(d_->ui->checkbox_rotate_keep_size, &QCheckBox::toggled,
            [this](bool){ options_changed_impl(); });
}

OcrSettingsWidget::~OcrSettingsWidget() = default;

void OcrSettingsWidget::set_options(const OcrOptions& options)
{
    d_->is_updating_from_code = true;
    d_->ui->checkbox_orientation_detect->setChecked(options.fix_page_orientation);
    d_->ui->spinbox_orientation_fraction->setValue(
                static_cast<int>(std::round(options.fix_page_orientation_min_text_fraction * 100)));
    d_->ui->spinbox_orientation_diff->setValue(
                static_cast<int>(std::round(rad_to_deg(options.fix_page_orientation_max_angle_diff))));

    d_->ui->checkbox_rotate_detect->setChecked(options.fix_text_rotation);
    d_->ui->spinbox_rotate_fraction->setValue(
                static_cast<int>(std::round(options.fix_text_rotation_min_text_fraction * 100)));
    d_->ui->spinbox_rotate_diff->setValue(
                static_cast<int>(std::round(rad_to_deg(options.fix_text_rotation_max_angle_diff))));
    d_->ui->checkbox_rotate_keep_size->setChecked(options.keep_image_size_after_rotation);
    d_->is_updating_from_code = false;
}

void OcrSettingsWidget::options_changed_impl()
{
    if (d_->is_updating_from_code) {
        return;
    }

    OcrOptions options;
    options.fix_page_orientation = d_->ui->checkbox_orientation_detect->isChecked();
    options.fix_page_orientation_min_text_fraction =
            d_->ui->spinbox_orientation_fraction->value() / 100.0;
    options.fix_page_orientation_max_angle_diff =
            deg_to_rad(d_->ui->spinbox_orientation_diff->value());

    options.fix_text_rotation = d_->ui->checkbox_rotate_detect->isChecked();
    options.fix_text_rotation_min_text_fraction = d_->ui->spinbox_rotate_fraction->value() / 100.0;
    options.fix_text_rotation_max_angle_diff = deg_to_rad(d_->ui->spinbox_rotate_diff->value());
    options.keep_image_size_after_rotation = d_->ui->checkbox_rotate_keep_size->isChecked();
    Q_EMIT options_changed(options);
}

} // namespace sanescan
