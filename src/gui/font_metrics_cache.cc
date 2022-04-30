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

#include "font_metrics_cache.h"
#include <QtGui/QFontMetrics>
#include <cmath>
#include <unordered_map>

namespace sanescan {

FontMetricsCache::Entry::Entry(const QFont& font, const QFontMetrics& metrics) :
    font{font},
    metrics{metrics}
{
}

struct FontMetricsCache::Private {
    QString font_family;
    std::unordered_map<int, FontMetricsCache::Entry> cached_fonts;
};

FontMetricsCache::FontMetricsCache(const std::string& font_family) :
    d_{std::make_unique<Private>()}
{
    d_->font_family = QString::fromStdString(font_family);
}

FontMetricsCache::~FontMetricsCache() = default;

const FontMetricsCache::Entry& FontMetricsCache::get_font_for_size(double font_size)
{
    int font_size_int = static_cast<int>(std::round(font_size));
    auto it = d_->cached_fonts.find(font_size_int);
    if (it != d_->cached_fonts.end()) {
        return it->second;
    }

    QFont font;
    font.setFamily(d_->font_family);
    font.setPixelSize(font_size_int);
    QFontMetrics metrics(font);

    auto [inserted_it, did_insert] = d_->cached_fonts.emplace(font_size_int, Entry{font, metrics});
    (void) did_insert;
    return inserted_it->second;
}

} // namespace sanescan
