/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Monika Kairaityte <monika@kibit.lt>

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

#include "about_dialog.h"
#include "ui_about_dialog.h"
#include "version.h"

namespace sanescan {

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui_{std::make_unique<Ui::AboutDialog>()}
{
    ui_->setupUi(this);
    setWindowTitle(tr("About sanescan"));

    QString description = tr("<h3>sanescan %1</h3>"
                             "Copyright 2021 Povilas Kanapickas &lt;povilas@radix.lt&gt; <br/>"
                             "Copyright 2021 Monika Kairaityte &lt;monika@kibit.lt&gt; <br/>"
                             "<br/>"
                             "This program is distributed in the hope that it will be useful, "
                             "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                             "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. <br/>")
            .arg(SANESCAN_VERSION);

    ui_->about_label->setText(description);
    ui_->about_label->setWordWrap(true);
}

AboutDialog::~AboutDialog() = default;

} // namespace sanescan
