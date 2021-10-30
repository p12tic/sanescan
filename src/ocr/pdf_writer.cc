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

    ----------------------------------------------------------------------
    The implementation has been partially based on pdfrenderer.cpp from the
    Tesseract OCR project <https://github.com/tesseract-ocr/tesseract>

    (C) Copyright 2011, Google Inc.
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "pdf_writer.h"
#include "pdf_canvas.h"
#include "pdf_ttf_font.h"

namespace sanescan {

PdfWriter::PdfWriter(std::ostream& stream) :
    output_dev_{&stream},
    doc_{&output_dev_, PoDoFo::ePdfVersion_1_5}
{
}

PdfWriter::~PdfWriter()
{
    doc_.Close();
}


void PdfWriter::write_header()
{
    type0_font_ = doc_.GetObjects()->CreateObject("Font");
    auto* cid_font_type2 = doc_.GetObjects()->CreateObject("Font");
    auto* cmap_file = doc_.GetObjects()->CreateObject();
    auto* cid_to_gid_map = doc_.GetObjects()->CreateObject();
    auto* font_descriptor = doc_.GetObjects()->CreateObject("FontDescriptor");
    auto* font_file = doc_.GetObjects()->CreateObject();

    setup_type0_font(type0_font_, cid_font_type2, cmap_file);
    setup_cid_system_info(cid_font_type2, cid_to_gid_map, font_descriptor);
    setup_cid_to_gid_map(cid_to_gid_map);
    setup_cmap_file(cmap_file);
    setup_font_descriptor(font_descriptor, font_file);
    setup_font_file(font_file);
}

void PdfWriter::write_page(const cv::Mat& image, const std::vector<OcrParagraph>& recognized)
{
    if (type0_font_ == nullptr) {
        throw std::runtime_error("write_header must be called before calling write_page");
    }

    auto width = image.size.p[1];
    auto height = image.size.p[0];

    auto* page = doc_.CreatePage(PoDoFo::PdfRect(0, 0, width, height));

    std::string font_ident = "font_ident";

    PoDoFo::PdfImage image_data(&doc_, "image-");
    page->AddResource(image_data.GetIdentifier(), image_data.GetObjectReference(), "XObject");
    page->AddResource(PoDoFo::PdfName(font_ident), type0_font_->Reference(), "Font");

    auto page_contents_data = get_contents_data_for_image(image_data.GetIdentifier().GetName(),
                                                      width, height);
    page_contents_data += get_contents_data_for_text(font_ident, width, height, recognized);

    PoDoFo::PdfMemoryInputStream page_contents_stream(page_contents_data.c_str(),
                                                       page_contents_data.size());
    page->GetContents()->GetStream()->SetRawData(&page_contents_stream);

    PoDoFo::PdfMemoryInputStream image_data_stream(reinterpret_cast<char*>(image.data),
                                                   image.elemSize1() *
                                                   image.total() * image.channels());

    image_data.SetImageColorSpace(PoDoFo::ePdfColorSpace_DeviceRGB);
    image_data.SetImageData(width, height, image.elemSize1() * 8, &image_data_stream);
}

void PdfWriter::setup_type0_font(PoDoFo::PdfObject* type0_font, PoDoFo::PdfObject* cid_font_type2,
                                 PoDoFo::PdfObject* cmap_file)
{
    PoDoFo::PdfArray type0_font_descendants;
    type0_font_descendants.push_back(cid_font_type2->Reference());

    type0_font->GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("GlyphLessFont"));
    type0_font->GetDictionary().AddKey("DescendantFonts", type0_font_descendants);
    type0_font->GetDictionary().AddKey("Encoding", PoDoFo::PdfName("Identity-H"));
    type0_font->GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type0"));
    type0_font->GetDictionary().AddKey("ToUnicode", cmap_file->Reference());
}

void PdfWriter::setup_cid_system_info(PoDoFo::PdfObject* cid_font_type2,
                                      PoDoFo::PdfObject* cid_to_gid_map,
                                      PoDoFo::PdfObject* font_descriptor)
{
    PoDoFo::PdfDictionary cid_system_info;
    cid_system_info.AddKey("Ordering", PoDoFo::PdfString("Identity"));
    cid_system_info.AddKey("Registry", PoDoFo::PdfString("Adobe"));
    cid_system_info.AddKey("Supplement", PoDoFo::PdfVariant(PoDoFo::pdf_int64(0)));

    cid_font_type2->GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("GlyphLessFont"));
    cid_font_type2->GetDictionary().AddKey("CIDToGIDMap", cid_to_gid_map->Reference());
    cid_font_type2->GetDictionary().AddKey("CIDSystemInfo", cid_system_info);
    cid_font_type2->GetDictionary().AddKey("FontDescriptor", font_descriptor->Reference());
    cid_font_type2->GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType2"));
    cid_font_type2->GetDictionary().AddKey("DW", PoDoFo::PdfVariant(
            PoDoFo::pdf_int64(1000 / CHAR_HEIGHT_DIVIDED_BY_WIDTH)));
}

void PdfWriter::setup_cid_to_gid_map(PoDoFo::PdfObject* cid_to_gid_map)
{
    const int kCIDToGIDMapSize = 2 * (1 << 16);
    std::vector<char> cid_to_gid_map_data;
    cid_to_gid_map_data.reserve(kCIDToGIDMapSize);
    for (int i = 0; i < kCIDToGIDMapSize; ++i) {
        // 0x0001 in big endian
        cid_to_gid_map_data.push_back(i % 2 ? 1 : 0);
    }

    cid_to_gid_map->GetStream()->Set(cid_to_gid_map_data.data(), cid_to_gid_map_data.size());
}

void PdfWriter::setup_cmap_file(PoDoFo::PdfObject* cmap_file)
{
    const char cmap_file_data[] =
        "/CIDInit /ProcSet findresource begin\n"
        "12 dict begin\n"
        "begincmap\n"
        "/CIDSystemInfo\n"
        "<<\n"
        "  /Registry (Adobe)\n"
        "  /Ordering (UCS)\n"
        "  /Supplement 0\n"
        ">> def\n"
        "/CMapName /Adobe-Identify-UCS def\n"
        "/CMapType 2 def\n"
        "1 begincodespacerange\n"
        "<0000> <FFFF>\n"
        "endcodespacerange\n"
        "1 beginbfrange\n"
        "<0000> <FFFF> <0000>\n"
        "endbfrange\n"
        "endcmap\n"
        "CMapName currentdict /CMap defineresource pop\n"
        "end\n"
        "end\n";

    cmap_file->GetStream()->Set(cmap_file_data, sizeof(cmap_file_data) - 1);
}

void PdfWriter::setup_font_descriptor(PoDoFo::PdfObject* font_descriptor,
                                      PoDoFo::PdfObject* font_file)
{
    PoDoFo::PdfArray font_bbox;
    font_bbox.push_back(PoDoFo::pdf_int64(0));
    font_bbox.push_back(PoDoFo::pdf_int64(0));
    font_bbox.push_back(PoDoFo::pdf_int64(1000 / CHAR_HEIGHT_DIVIDED_BY_WIDTH));
    font_bbox.push_back(PoDoFo::pdf_int64(1000));

    font_descriptor->GetDictionary().AddKey("Ascent", PoDoFo::PdfVariant(PoDoFo::pdf_int64(800)));
    font_descriptor->GetDictionary().AddKey("CapHeight", PoDoFo::PdfVariant(PoDoFo::pdf_int64(800)));
    font_descriptor->GetDictionary().AddKey("Descent", PoDoFo::PdfVariant(PoDoFo::pdf_int64(-200)));
    font_descriptor->GetDictionary().AddKey("Flags", PoDoFo::PdfVariant(PoDoFo::pdf_int64(5)));
    font_descriptor->GetDictionary().AddKey("FontBBox", font_bbox);
    font_descriptor->GetDictionary().AddKey("FontFile2", font_file->Reference());
    font_descriptor->GetDictionary().AddKey("FontName", PoDoFo::PdfName("GlyphLessFont"));
    font_descriptor->GetDictionary().AddKey("ItalicAngle", PoDoFo::PdfVariant(PoDoFo::pdf_int64(0)));
    font_descriptor->GetDictionary().AddKey("StemV", PoDoFo::PdfVariant(PoDoFo::pdf_int64(80)));
}

void PdfWriter::setup_font_file(PoDoFo::PdfObject* font_file)
{
    auto data_size = sizeof(pdf_ttf_font_data);
    font_file->GetDictionary().AddKey("Length1",
                                      PoDoFo::PdfVariant(PoDoFo::pdf_int64(data_size)));
    font_file->GetStream()->Set(reinterpret_cast<const char*>(pdf_ttf_font_data), data_size);
}

std::string PdfWriter::get_contents_data_for_image(const std::string& image_name,
                                                   double width, double height)
{
    PdfCanvas canvas;
    canvas.save_state();
    canvas.set_ctm(width, 0, 0, height, 0, 0);
    canvas.draw_object(image_name);
    canvas.restore_state();
    canvas.separator();

    return canvas.get_string();
}

std::string PdfWriter::get_contents_data_for_text(const std::string& font_ident,
                                                  double width, double height,
                                                  const std::vector<OcrParagraph>& recognized)
{
    PdfCanvas canvas;
    std::vector<double> text_adjustments;

    for (const auto& par : recognized) {
        for (const auto& line : par.lines) {
            canvas.begin_text();
            canvas.set_text_mode_outline();

            auto matrix = compute_affine_matrix_for_line(line.baseline_angle);
            auto line_baseline_x = line.box.x1 + line.baseline_x;
            auto line_baseline_y = height - line.box.y2 - line.baseline_y;
            canvas.set_text_matrix(matrix.a, matrix.b, matrix.c, matrix.d,
                                   line_baseline_x, line_baseline_y);
            double old_x = line_baseline_x;
            double old_y = line_baseline_y;
            double old_fontsize = -1;

            for (const auto& word : line.words) {
                auto text_utf32 = boost::locale::conv::utf_to_utf<char32_t>(word.content);
                if (text_utf32.empty()) {
                    continue;
                }

                double word_x = word.box.x1;
                double word_y = line_baseline_y - (word_x - line_baseline_x) * std::tan(line.baseline_angle);
                double dx = word_x - old_x;
                double dy = word_y - old_y;
                canvas.translate_text_matrix(dx * matrix.a + dy * matrix.b,
                                             dx * matrix.c + dy * matrix.d);
                old_x = word_x;
                old_y = word_y;

                bool use_char_positioning = text_utf32.size() == word.char_boxes.size();

                auto font_size = word.font_size > 1 ? word.font_size : FALL_BACK_FONT_SIZE;
                if (font_size != old_fontsize) {
                    canvas.set_font(font_ident, font_size);
                    old_fontsize = font_size;
                }

                if (use_char_positioning) {
                    // This will be most frequent case as the input data will always have
                    // the number of char boxes equal to the number of symbols.
                    auto font_char_width = double(font_size) / CHAR_HEIGHT_DIVIDED_BY_WIDTH;
                    double old_x = word.box.x1;

                    for (std::size_t i = 1; i < text_utf32.size(); ++i) {
                        double x = word.char_boxes[i].x1;
                        double curr_char_width = x - old_x;
                        old_x = x;
                        auto stretch_percent = 100 * curr_char_width / font_char_width;
                        canvas.set_horizontal_stretch(stretch_percent);
                        canvas.show_text(text_utf32.substr(i - 1, 1));
                    }

                    {
                        double curr_char_width = word.box.x2 - old_x;
                        auto stretch_percent = 100 * curr_char_width / font_char_width;
                        canvas.set_horizontal_stretch(stretch_percent);
                        canvas.show_text(text_utf32.substr(text_utf32.size() - 1, 1));
                    }
                    canvas.separator();

                    // Add space character.
                    {
                        canvas.set_horizontal_stretch(20);
                        canvas.show_text(std::u32string({' '}));
                        canvas.separator();
                        canvas.set_horizontal_stretch(100);
                    }

                } else {
                    // Fallback in case the number symbols have been adjusted. We compute
                    // the amount of space the font would use for the given number of
                    // characters and then adjust horizontal stretch so that the actual space
                    // use is exactly equal to how much space we have.
                    auto word_dx = word.box.x2 - word.box.x1;
                    auto word_baseline_length = std::hypot(word_dx,
                                                           word_dx * std::tan(word.baseline_angle));
                    auto font_char_width = double(font_size) / CHAR_HEIGHT_DIVIDED_BY_WIDTH;
                    auto curr_char_width = word_baseline_length / text_utf32.size();
                    auto stretch_percent = 100 * curr_char_width / font_char_width;

                    canvas.set_horizontal_stretch(stretch_percent);
                    canvas.show_text(text_utf32);

                    // We need to also emit a space after the word, but we don't know how much
                    // free space we have, so we use very small stretch to make sure the space
                    // character does not overlap the next word.
                    canvas.set_horizontal_stretch(stretch_percent * 0.2);
                    canvas.show_text(std::u32string({' '}));
                    canvas.separator();
                }
                canvas.end_text();
            }
        }
    }

    return canvas.get_string();
}

} // namespace sanescan
