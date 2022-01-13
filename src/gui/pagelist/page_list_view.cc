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

#include "page_list_view.h"
#include "page_list_model.h"
#include <QtGui/QResizeEvent>
#include <stdexcept>

namespace sanescan {

struct PageListView::Private {
    PageListModel* model = nullptr;
};

PageListView::PageListView(QWidget* parent) :
    QListView{parent},
    d_{std::make_unique<Private>()}
{
}

PageListView::~PageListView() = default;

void PageListView::setModel(QAbstractItemModel* model)
{
    auto* list_model = dynamic_cast<PageListModel*>(model);
    if (list_model == nullptr) {
        throw std::invalid_argument("PageListView only supports PageListModel");
    }
    d_->model = list_model;
    QListView::setModel(model);
}

const QPixmap& PageListView::image_for_item(const QModelIndex& index)
{
    if (d_->model == nullptr) {
        throw std::runtime_error("Can't acquire image when no model is set");
    }
    return d_->model->image_at(index.row());
}

void PageListView::resizeEvent(QResizeEvent* event)
{
    d_->model->set_image_sizes(event->size().width());
}

} // namespace sanescan
