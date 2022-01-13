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

#ifndef SANESCAN_GUI_PAGELIST_PAGE_LIST_VIEW_DELEGATE_H
#define SANESCAN_GUI_PAGELIST_PAGE_LIST_VIEW_DELEGATE_H

#include <QtWidgets/QAbstractItemDelegate>
#include <memory>

namespace sanescan {

class PageListView;

class PageListViewDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:
    PageListViewDelegate(PageListView* list);
    ~PageListViewDelegate() override;

    void paint(QPainter *painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_PAGELIST_PAGE_LIST_VIEW_DELEGATE_H
