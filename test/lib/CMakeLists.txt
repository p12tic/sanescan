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

set(SOURCES
    main.cc
)

include(FindPkgConfig)
find_package(GTest REQUIRED)
pkg_check_modules(GMOCK REQUIRED gmock)
include_directories(
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

add_executable(libtest ${SOURCES})

target_link_libraries(libtest
    ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}
    Threads::Threads
    sanescanlib
)
