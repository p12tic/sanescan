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


#ifndef SANESCAN_OCR_PDF_CANVAS_H
#define SANESCAN_OCR_PDF_CANVAS_H

#include <boost/locale/encoding.hpp>
#include <cmath>
#include <sstream>
#include <vector>

namespace sanescan {

struct AffineMatrix
{
    double a = 0;
    double b = 0;
    double c = 0;
    double d = 0;
};

inline AffineMatrix compute_affine_matrix_for_line(double line_angle)
{
    auto angle = -line_angle;
    return {
        std::cos(angle),
        std::sin(angle),
        -std::sin(angle),
        std::cos(angle)
    };
}

// For details see PDF 32000-1:2008
class PdfCanvas
{
public:
    // See section "Text Rendering Mode" of the PDF standard (e.g. section 9.3.6 in PDF32000)
    enum class TextMode {
        FILL,
        STROKE,
        FILL_STROKE,
        INVISIBLE,
        FILL_CLIP,
        STROKE_CLIP,
        FILL_STROKE_CLIP,
        CLIP
    };

    PdfCanvas()
    {
        // force classic locales so that it uses correct dot separator for double values
        str_.imbue(std::locale::classic());
        // PDF does not support scientific notation
        str_ << std::fixed;
    }

    void set_ctm(double a, double b, double c, double d, double e, double f)
    {
        maybe_write_space();
        str_ << prec6(a) << " " << prec6(b) << " "
             << prec6(c) << " " << prec6(d) << " "
             << prec6(e) << " " << prec6(f) << " cm";
    }

    void set_text_matrix(double a, double b, double c, double d, double e, double f)
    {
        maybe_write_space();
        str_ << prec6(a) << " " << prec6(b) << " "
             << prec6(c) << " " << prec6(d) << " "
             << prec6(e) << " " << prec6(f) << " Tm";
    }

    void translate_text_matrix(double dx, double dy)
    {
        maybe_write_space();
        str_ << prec6(dx) << " " << prec6(dy) << " Td";
    }

    void draw_object(const std::string& object_name)
    {
        maybe_write_space();
        str_ << "/" << object_name << " " << "Do";
    }

    void save_state()
    {
        maybe_write_space();
        str_ << "q";
    }

    void restore_state()
    {
        maybe_write_space();
        str_ << "Q";
    }

    void begin_text()
    {
        maybe_write_space();
        str_ << "BT";
    }

    void end_text()
    {
        maybe_write_space();
        str_ << "ET";
    }

    void set_text_mode(TextMode mode)
    {
        maybe_write_space();
        switch (mode) {
            case TextMode::FILL:
                str_ << "0 Tr"; break;
            case TextMode::STROKE:
                str_ << "1 Tr"; break;
            case TextMode::FILL_STROKE:
                str_ << "2 Tr"; break;
            case TextMode::INVISIBLE:
                str_ << "3 Tr"; break;
            case TextMode::FILL_CLIP:
                str_ << "4 Tr"; break;
            case TextMode::STROKE_CLIP:
                str_ << "5 Tr"; break;
            case TextMode::FILL_STROKE_CLIP:
                str_ << "6 Tr"; break;
            case TextMode::CLIP:
                str_ << "7 Tr"; break;
        }
    }

    void set_font(const std::string& name, double size)
    {
        maybe_write_space();
        str_ << "/" << name << " " << prec6(size) << " Tf";
    }

    void set_horizontal_stretch(double stretch)
    {
        maybe_write_space();
        str_ << prec6(stretch) << " Tz";
    }

    /// Caller must ensure that the text is indeed ASCII
    void show_text_ascii(const std::string& text)
    {
        maybe_write_space();
        str_ << "(";
        for (auto ch : text) {
            if (!std::isalnum(ch) && !std::ispunct(ch)) {
                throw std::invalid_argument("Text must be ASCII without control characters");
            }

            switch (ch) {
                case '(': str_ << "\\("; break;
                case ')': str_ << "\\)"; break;
                case '\\': str_ << "\\\\"; break;
                default: str_ << ch; break;
            }
        }

        str_ << ") Tj";
    }

    void show_text(const std::u32string& utf32_text)
    {
        maybe_write_space();
        str_ << "<";

        constexpr unsigned hex_size = 5;
        char hex_ch[hex_size] = {};

        auto utf16 = boost::locale::conv::utf_to_utf<char16_t>(utf32_text);
        for (auto ch : utf16) {
            std::snprintf(hex_ch, hex_size, "%04X", static_cast<int>(ch));
            str_ << hex_ch;
        }

        str_ << "> Tj";
    }

    void show_text_with_positions(const std::u32string& utf32_text,
                                  const std::vector<double>& position_adjustments)
    {
        maybe_write_space();
        str_ << "[";

        if (utf32_text.size() != position_adjustments.size()) {
            throw std::runtime_error("Character and their adjustment count must match");
        }

        constexpr unsigned hex_size = 5;
        char hex_ch[hex_size] = {};

        auto utf16 = boost::locale::conv::utf_to_utf<char16_t>(utf32_text);
        for (std::size_t i = 0; i < utf32_text.size(); ++i) {
            str_ << prec6(position_adjustments[i]) << "<";
            std::snprintf(hex_ch, hex_size, "%04X", static_cast<int>(utf32_text[i]));
            str_ << hex_ch;
            str_ << ">";
        }

        str_ << "] TJ";
    }

    void separator()
    {
        str_ << "\n";
        needs_space_ = false;
    }

    std::string get_string() const
    {
        return str_.str();
    }

private:
    void maybe_write_space()
    {
        if (!needs_space_) {
            needs_space_ = true;
            return;
        }
        str_ << " ";
    }

    static double prec6(double x)
    {
        double precision = 1e6;
        double r = std::round(x * precision) / precision;
        if (r == -0) {
            return 0;
        }
        return r;
    }

    std::ostringstream str_;
    bool needs_space_ = false;
};

} // namespace sanescan

#endif // SANESCAN_OCR_PDF_H
