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
#include <cstring>
#include <string>
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
            line.baseline_y * line.baseline_coeff * (word.box.x1 - line.box.x1);
    word.baseline_coeff = line.baseline_coeff;
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
    line.baseline_coeff = baseline_values[0];
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

} // namespace sanescan
