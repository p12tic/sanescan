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
#include "ui_main_window.h"

namespace sanescan {

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui_{std::make_unique<Ui::MainWindow>()}
{
    ui_->setupUi(this);

    connect(ui_->action_about, &QAction::triggered, [this](){ present_about_dialog(); });

    ui_->image_area->set_image(QImage("test.jpeg"));
}

MainWindow::~MainWindow() = default;

void MainWindow::present_about_dialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}

} // namespace sanescan
