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

struct PageImages {
    PageImages(const QImage& image) : image{image} {}

    void resize(unsigned new_height)
    {
        QPixmap pix = QPixmap::fromImage(image);
        resized_pixmap = pix.scaledToHeight(new_height);
    }

    QImage image;
    QPixmap resized_pixmap;
};

struct PageListModel::Private {
    std::vector<std::uint64_t> pages;
    std::map<std::uint64_t, PageImages> images;
    unsigned pixmap_height = 200;
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

void PageListModel::add_page(std::uint64_t identifier, const QImage& image)
{
    PageImages page_images{image};
    page_images.resize(d_->pixmap_height);

    d_->pages.push_back(identifier);
    d_->images.emplace(identifier, std::move(page_images));
    Q_EMIT layoutChanged();
}

void PageListModel::set_image(std::uint64_t identifier, const QImage& image)
{
    auto it = d_->images.find(identifier);
    if (it == d_->images.end()) {
        throw std::runtime_error("Image for identifier does not exist");
    }
    it->second.image = image;
    it->second.resize(d_->pixmap_height);

    auto it_pages = std::find(d_->pages.begin(), d_->pages.end(), identifier);
    if (it_pages == d_->pages.end()) {
        throw std::runtime_error("Page for identifier does not exist");
    }
    auto pos = std::distance(d_->pages.begin(), it_pages);

    Q_EMIT dataChanged(index(pos), index(pos));
}

const QPixmap& PageListModel::image_at(std::size_t pos) const
{
    auto identifier = d_->pages.at(pos);
    auto it = d_->images.find(identifier);
    if (it == d_->images.end()) {
        throw std::runtime_error("Image for index does not exist");
    }
    return it->second.resized_pixmap;
}

void PageListModel::set_image_sizes(unsigned height)
{
    if (height == d_->pixmap_height) {
        return;
    }
    d_->pixmap_height = height;
    for (auto& [ident, images] : d_->images) {
        images.resize(height);
    }
}

} // namespace sanescan
