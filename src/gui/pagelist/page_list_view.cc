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

constexpr unsigned LIST_ITEM_PADDING = 8;

struct PageListView::Private {
    PageListModel* model = nullptr;
};

PageListView::PageListView(QWidget* parent) :
    QListView{parent},
    d_{std::make_unique<Private>()}
{
    setFlow(QListView::Flow::LeftToRight);

    // TODO: take into account themes somehow
    setStyleSheet(R"(
QListView {
    background-color: #a0a0a0;
}

QListView::item {
    background-color: #606060;
    border-top: 1px solid #202020;
    border-bottom: 1px solid #202020;
}

QListView::item:selected {
    background-color: #4040f0;
}

QListView::item:hover:!selected {
    background-color: #8080f0;
}
)");
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

unsigned PageListView::list_item_padding() const
{
    return LIST_ITEM_PADDING;
}

void PageListView::resizeEvent(QResizeEvent* event)
{
    auto max_height = event->size().height() - 2 * LIST_ITEM_PADDING;
    // We want landscape pages to be displayed as large as possible, but any images with larger
    // aspect ratio will be resized to be smaller so that they don't expand across whole page list.
    auto max_width = max_height * 1.41421;

    d_->model->set_max_image_size(QSize(max_width, max_height));
    scheduleDelayedItemsLayout();
}

} // namespace sanescan
