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

#ifndef SANESCAN_GUI_PAGELIST_PAGE_LIST_MODEL_H
#define SANESCAN_GUI_PAGELIST_PAGE_LIST_MODEL_H

#include <QtCore/QAbstractListModel>
#include <memory>

namespace sanescan {

class PageListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit PageListModel(QObject* parent);
    ~PageListModel() override;

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void add_page(std::uint64_t identifier, const QImage& image);

    const QPixmap& image_at(std::size_t pos) const;

    void set_image_sizes(unsigned width);

private:
    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif // SANESCAN_GUI_PAGELIST_PAGE_LIST_MODEL_H
