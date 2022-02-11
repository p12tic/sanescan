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

include(FindPkgConfig)
find_package(Threads)
find_package(OpenCV REQUIRED)
pkg_check_modules(poppler REQUIRED poppler-cpp)

set(SOURCES
    buffer_manager.cc
    file_loader_image.cc
    file_loader_pdf.cc
    sane_device_wrapper.cc
    sane_types.cc
    sane_utils.cc
    sane_wrapper.cc
    scan_image_buffer.cc
    task_executor.cc
)

add_library(sanescanlib OBJECT ${SOURCES})

target_link_libraries(sanescanlib PUBLIC
    Threads::Threads
    ${OpenCV_LIBS}
    ${poppler_LIBRARIES}
    sane
)

target_include_directories(sanescanlib PUBLIC
    ${OpenCV_INCLUDE_DIRS}
)
