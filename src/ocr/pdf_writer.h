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

#ifndef SANESCAN_OCR_PDF_WRITER_H
#define SANESCAN_OCR_PDF_WRITER_H

#include "fwd.h"
#include "ocr_paragraph.h"
#include "pdf.h"
#include <opencv2/core/mat.hpp>
#include <podofo/podofo.h>
#include <utility>

namespace sanescan {

class PdfWriter
{
public:
    static constexpr int CHAR_HEIGHT_DIVIDED_BY_WIDTH = 2;
    static constexpr double FALL_BACK_FONT_SIZE = 10;

    PdfWriter(std::ostream& stream, WritePdfFlags flags = WritePdfFlags::NONE);
    ~PdfWriter();

    void write_header();
    void write_page(const cv::Mat& image, const std::vector<OcrParagraph>& recognized);

private:
    void setup_type0_font(PoDoFo::PdfObject* type0_font, PoDoFo::PdfObject* cid_font_type2,
                          PoDoFo::PdfObject* cmap_file);

    void setup_cid_system_info(PoDoFo::PdfObject* cid_font_type2, PoDoFo::PdfObject* cid_to_gid_map,
                               PoDoFo::PdfObject* font_descriptor);
    void setup_cid_to_gid_map(PoDoFo::PdfObject* cid_to_gid_map);

    void setup_cmap_file(PoDoFo::PdfObject* cmap_file);

    void setup_font_descriptor(PoDoFo::PdfObject* font_descriptor, PoDoFo::PdfObject* font_file);
    void setup_font_file(PoDoFo::PdfObject* font_file);

    std::string get_contents_data_for_image(const std::string& image_name,
                                            double width, double height);
    std::string get_contents_data_for_text(const std::string& font_ident,
                                           double width, double height,
                                           const std::vector<OcrParagraph>& recognized);

    void write_line_to_canvas(PdfCanvas& canvas, const std::string& font_ident,
                              double width, double height, const OcrLine& line,
                              std::size_t paragraph_index, std::size_t line_index);

    std::pair<double, double> adjust_small_baseline_angle(const OcrLine& line);

    PoDoFo::PdfOutputDevice output_dev_;
    PoDoFo::PdfStreamedDocument doc_;
    PoDoFo::PdfObject* type0_font_ = nullptr;
    PoDoFo::PdfFont* debug_font_ = nullptr;
    WritePdfFlags flags_;
};

} // namespace sanescan

#endif // SANESCAN_OCR_PDF_WRITER_H
