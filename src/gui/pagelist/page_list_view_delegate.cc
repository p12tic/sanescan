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

#include "page_list_view_delegate.h"
#include "page_list_view.h"
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

namespace sanescan {

struct PageListViewDelegate::Private {
    PageListView* list = nullptr;
};

PageListViewDelegate::PageListViewDelegate(PageListView* list) :
    d_{std::make_unique<Private>()}
{
    d_->list = list;
}

PageListViewDelegate::~PageListViewDelegate() = default;


void PageListViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    const auto& pix = d_->list->image_for_item(index);
    QSize pix_size = pix.size();
    QRect rect = option.rect;

    QRect pix_rect = QRect(rect.left() + (rect.width() - pix_size.width()) / 2,
                           rect.top() + (rect.height() - pix_size.height()) / 2,
                           pix_size.width(), pix_size.height());

    QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);

    painter->drawPixmap(pix_rect.left(), pix_rect.top(), pix);
}

QSize PageListViewDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    auto padding = d_->list->list_item_padding();
    const auto& pix = d_->list->image_for_item(index);
    return pix.size() + 2 * QSize(padding, padding);
}

} // namespace sanescan
