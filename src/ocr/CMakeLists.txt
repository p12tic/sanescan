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
find_package(OpenCV REQUIRED)
pkg_check_modules(tesseract REQUIRED tesseract)

set(SOURCES
    blur_detection.cc
    hocr.cc
    ocr_baseline.cc
    ocr_box.cc
    ocr_line.cc
    ocr_paragraph.cc
    ocr_results_evaluator.cc
    ocr_word.cc
    ocr_utils.cc
    pdf.cc
    pdf_writer.cc
    tesseract.cc
    tesseract_renderer.cc
    ../util/image.cc
)

add_library(sanescanocr OBJECT ${SOURCES})

target_link_libraries(sanescanocr PUBLIC
    leptonica
    ${tesseract_LIBRARIES}
    pugixml
    podofo
    ${OpenCV_LIBS}
)

target_include_directories(sanescanocr PUBLIC
    ${OpenCV_INCLUDE_DIRS}
)
