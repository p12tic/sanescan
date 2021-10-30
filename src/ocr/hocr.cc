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

#include "hocr.h"
#include "hocr_private.h"
#include <pugixml.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cmath>
#include <cstring>
#include <string>
#include <sstream>
#include <optional>
#include <unordered_map>

namespace sanescan {

namespace internal {

double parse_double_or_exception(const std::string& input)
{
    const char* c_str = input.c_str();
    char* end = nullptr;
    auto value = std::strtod(c_str, &end);
    if (end == c_str) {
        throw HocrException("Could not parse " + input + " as floating-point value");
    }
    return value;
}

HocrProps parse_hocr_props(const char* attr_value)
{
    HocrProps result;

    std::vector<std::string> kv_parts;
    boost::split(kv_parts, attr_value, boost::is_any_of(";"));

    std::vector<std::string> parts;
    for (auto& kv_part : kv_parts) {
        boost::trim(kv_part);
        boost::split(parts, kv_part, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() < 2) {
            continue;
        }

        auto name = parts[0];
        std::vector<double> values;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            values.push_back(parse_double_or_exception(parts[i].c_str()));
        }
        result[name] = std::move(values);
    }
    return result;
}

const std::vector<double>& get_hocr_values_or_exception(const HocrProps& props,
                                                        const char* prop_name,
                                                        std::size_t expected_size)
{
    auto prop_it = props.find(prop_name);
    if (prop_it == props.end()) {
        throw HocrException("Could not find HOCR property: " + std::string(prop_name));
    }
    auto& values = prop_it->second;
    if (values.size() != expected_size) {
        throw HocrException("Unexpected number of values for HOCR property " +
                            std::string(prop_name) + " " + std::to_string(values.size()));
    }
    return values;
}

OcrBox parse_hocr_box(const HocrProps& props, const char* prop_name)
{
    const auto& values = get_hocr_values_or_exception(props, prop_name, 4);
    auto left = values[0];
    auto top = values[1];
    auto right = values[2];
    auto bottom = values[3];
    return OcrBox{static_cast<std::int32_t>(left),
                  static_cast<std::int32_t>(top),
                  static_cast<std::int32_t>(right),
                  static_cast<std::int32_t>(bottom)};
}

OcrWord parse_hocr_word(pugi::xml_node e_word, const OcrLine& line, double font_size)
{
    OcrWord word;

    auto props = parse_hocr_props(e_word.attribute("title").value());
    word.box = parse_hocr_box(props, "bbox");
    word.confidence = get_hocr_values_or_exception(props, "x_wconf", 1)[0];

    word.baseline_y = (word.box.y2 - line.box.y2) +
            line.baseline_y * std::tan(line.baseline_angle) * (word.box.x1 - line.box.x1);
    word.baseline_angle = line.baseline_angle;
    word.font_size = font_size;

    for (auto e_cinfo : e_word.children("span")) {
        if (std::strcmp(e_cinfo.attribute("class").value(), "ocrx_cinfo") != 0) {
            continue;
        }
        auto char_props = parse_hocr_props(e_cinfo.attribute("title").value());
        word.char_boxes.push_back(parse_hocr_box(char_props, "x_bboxes"));
        word.content += e_cinfo.text().as_string();
    }
    return word;
}

OcrLine parse_hocr_line(pugi::xml_node e_line)
{
    OcrLine line;

    auto props = parse_hocr_props(e_line.attribute("title").value());
    line.box = parse_hocr_box(props, "bbox");

    const auto& baseline_values = get_hocr_values_or_exception(props, "baseline", 2);
    line.baseline_angle = std::atan(baseline_values[0]);
    line.baseline_y = baseline_values[1];
    double font_size = get_hocr_values_or_exception(props, "x_size", 1)[0];

    for (auto e_word : e_line.children("span")) {
        if (std::strcmp(e_word.attribute("class").value(), "ocrx_word") != 0) {
            continue;
        }
        auto word = parse_hocr_word(e_word, line, font_size);
        if (!word.char_boxes.empty()) {
            line.words.push_back(std::move(word));
        }
    }

    return line;
}

OcrParagraph parse_hocr_paragraph(pugi::xml_node e_par)
{
    OcrParagraph paragraph;

    for (auto e_line : e_par.children("span")) {
        if (std::strcmp(e_line.attribute("class").value(), "ocr_line") != 0) {
            continue;
        }
        auto props = parse_hocr_props(e_line.attribute("title").value());
        paragraph.box = parse_hocr_box(props, "bbox");

        auto line = parse_hocr_line(e_line);
        if (!line.words.empty()) {
            paragraph.lines.push_back(std::move(line));
        }
    }

    return paragraph;
}

} // namespace internal

// Note that because rapidxml modifies the input string in-place we do want to make a copy of the
// input here.
std::vector<OcrParagraph> read_hocr(std::istream& input)
{
    pugi::xml_document doc;
    auto load_result = doc.load(input);
    if (!load_result) {
        throw HocrException(std::string{"Could not parse input document "} +
                            load_result.description());
    }

    std::vector<OcrParagraph> result;

    auto e_body = doc.child("html").child("body");
    if (!e_body) {
        throw HocrException("Input document does not contain body element");
    }

    for (auto e_page : e_body.children("div")) {
        if (std::strcmp(e_page.attribute("class").value(), "ocr_page") != 0) {
            continue;
        }

        for (auto e_carea : e_page.children("div")) {
            if (std::strcmp(e_carea.attribute("class").value(), "ocr_carea") != 0) {
                continue;
            }

            for (auto c_par : e_carea.children("p")) {
                if (std::strcmp(c_par.attribute("class").value(), "ocr_par") != 0) {
                    continue;
                }

                auto parsed_par = internal::parse_hocr_paragraph(c_par);
                if (!parsed_par.lines.empty()) {
                    result.push_back(std::move(parsed_par));
                }
            }
        }
    }
    return result;
}

std::string box_to_hocr(const OcrBox& box)
{
    return std::to_string(box.x1) + " " +
            std::to_string(box.y1) + " " +
            std::to_string(box.x2) + " " +
            std::to_string(box.y2);
}

void write_hocr(std::ostream& output, const std::vector<OcrParagraph>& paragraphs)
{
    pugi::xml_document doc;
    auto e_html = doc.append_child("html");
    e_html.append_attribute("xmlns") = "http://www.w3.org/1999/xhtml";
    e_html.append_attribute("xml:lang") = "en";
    e_html.append_attribute("lang") = "en";

    auto e_head = e_html.append_child("head");
    e_head.append_child("title");

    auto e_meta = e_head.append_child("meta");
    e_meta.append_attribute("http-equiv") = "Content-Type";
    e_meta.append_attribute("content") = "text/html;charset=utf-8";

    e_meta = e_head.append_child("meta");
    e_meta.append_attribute("name") = "ocr-system";
    e_meta.append_attribute("content") = "sanescan";

    e_meta = e_head.append_child("meta");
    e_meta.append_attribute("name") = "ocr-capabilities";
    e_meta.append_attribute("content") = "ocr_page ocr_carea ocr_par ocr_line ocrx_word ocrp_wconf";

    auto e_body = e_head.append_child("body");
    auto e_page = e_body.append_child("div");
    e_page.append_attribute("class") = "ocr_page";

    auto e_carea = e_page.append_child("div");
    e_carea.append_attribute("class") = "ocr_carea";

    for (const auto& par : paragraphs) {

        auto e_p = e_carea.append_child("p");
        e_p.append_attribute("class") = "ocr_par";
        e_p.append_attribute("lang") = "eng";

        std::ostringstream par_title;
        par_title << "bbox " << box_to_hocr(par.box);
        e_p.append_attribute("title") = par_title.str().c_str();

        for (const auto& line : par.lines) {
            if (line.words.empty()) {
                continue;
            }

            auto e_line = e_p.append_child("span");
            e_line.append_attribute("class") = "ocr_line";

            std::ostringstream line_title;
            line_title << "bbox " << box_to_hocr(line.box) << ";"
                       << " baseline " << std::tan(line.baseline_angle)
                                       << " " << line.baseline_y << ";"
                       << " x_size " << line.words.front().font_size;
            e_line.append_attribute("title") = line_title.str().c_str();

            for (const auto& word : line.words) {
                auto e_word = e_line.append_child("span");
                e_word.append_attribute("class") = "ocrx_word";

                std::ostringstream word_title;
                word_title << "bbox " << box_to_hocr(word.box) << ";"
                           << " x_wconf " << word.confidence * 100;
                e_word.append_attribute("title") = word_title.str().c_str();

                for (const auto& ch_box : word.char_boxes) {
                    // TODO: character strings are not saved
                    auto e_ch = e_word.append_child("span");
                    e_ch.append_attribute("class") = "ocrx_cinfo";

                    std::ostringstream ch_title;
                    ch_title << "x_bboxes " << box_to_hocr(ch_box);
                    e_ch.append_attribute("title") = ch_title.str().c_str();
                }
            }
        }
    }

    doc.save(output);
}

} // namespace sanescan
