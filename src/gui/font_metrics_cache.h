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

#ifndef SANESCAN_GUI_FONT_METRICS_CACHE_H
#define SANESCAN_GUI_FONT_METRICS_CACHE_H

#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <memory>
#include <optional>

namespace sanescan {

class FontMetricsCache
{
public:
    struct Entry {
        Entry(const QFont& font, const QFontMetrics& metrics);

        QFont font;
        QFontMetrics metrics;
    };

    FontMetricsCache(const std::string& font_family);
    ~FontMetricsCache();

    const Entry& get_font_for_size(double font_size);

private:
    struct Private;
    std::unique_ptr<Private> d_;
};


} // namespace sanescan
#endif // SANESCAN_GUI_VERSION_DIALOG_H
