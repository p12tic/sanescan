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

#include "page_list_model.h"
#include <QtGui/QPixmap>
#include <stdexcept>

namespace sanescan {

struct PageListModel::Private {
    std::vector<std::uint64_t> pages;
    std::map<std::uint64_t, QPixmap> images;
};

PageListModel::PageListModel(QObject* parent) :
    QAbstractListModel{parent},
    d_{std::make_unique<Private>()}
{
}

PageListModel::~PageListModel() = default;

int PageListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        // If we return nonzero value here we will turn the model into a tree model.
        return 0;
    }

    return d_->pages.size();
}

QVariant PageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    return {static_cast<int>(d_->pages.at(index.row()))};
}

void PageListModel::add_page(std::uint64_t identifier, QPixmap image)
{
    d_->pages.push_back(identifier);
    d_->images.emplace(identifier, image);
    Q_EMIT layoutChanged();
}

const QPixmap& PageListModel::image_at(std::size_t pos) const
{
    auto identifier = d_->pages.at(pos);
    auto it = d_->images.find(identifier);
    if (it == d_->images.end()) {
        throw std::runtime_error("Image for index does not exist");
    }
    return it->second;
}

} // namespace sanescan
