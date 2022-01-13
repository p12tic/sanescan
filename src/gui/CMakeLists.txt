# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2021  Povilas Kanapickas <povilas@radix.lt>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 COMPONENTS Core Widgets)
find_package(Threads)

set(SOURCES
    about_dialog.cc
    about_dialog.ui
    image_widget.cc
    main.cc
    main_window.cc
    main_window.ui
    qimage_converter.cc
    scan_engine.cc
    scan_settings_widget.cc
    scan_settings_widget.ui
    pagelist/page_list_model.cc
    pagelist/page_list_view.cc
    pagelist/page_list_view_delegate.cc
    settings/setting_combo.cc
    settings/setting_combo.ui
    settings/setting_spin.cc
    settings/setting_spin.ui
    settings/setting_spin_float.cc
    settings/setting_spin_float.ui
    settings/setting_widget.cc
)

configure_file(version.h.in version.h)

add_executable(sanescan ${SOURCES})

target_include_directories(sanescan PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(sanescan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sanescan
    Qt5::Core
    Qt5::Widgets
    Threads::Threads
    sanescanlib
)
