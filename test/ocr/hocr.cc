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

#include "ocr/hocr.h"
#include "ocr/hocr_private.h"
#include <gtest/gtest.h>

namespace sanescan {

TEST(Hocr, ParseHocrProps)
{
    internal::HocrProps expected;
    expected = {};
    ASSERT_EQ(internal::parse_hocr_props(""), expected);
    ASSERT_EQ(internal::parse_hocr_props("name"), expected);
    ASSERT_EQ(internal::parse_hocr_props("name;"), expected);
    ASSERT_EQ(internal::parse_hocr_props(";;name;"), expected);
    ASSERT_THROW(internal::parse_hocr_props("name prop"), HocrException);

    expected = {{"name", {1}}};
    ASSERT_EQ(internal::parse_hocr_props("name 1"), expected);
    expected = {{"name", {1.5}}};
    ASSERT_EQ(internal::parse_hocr_props("name 1.5"), expected);
    expected = {{"name", {1.5, 2.5}}};
    ASSERT_EQ(internal::parse_hocr_props("name 1.5 2.5"), expected);
    ASSERT_EQ(internal::parse_hocr_props("name 1.5 2.5;"), expected);
    ASSERT_EQ(internal::parse_hocr_props("name 1.5 2.5; name2"), expected);

    expected = {{"name", {1.5, 2.5}}, {"name2", {3.5, 4.5}}};
    ASSERT_EQ(internal::parse_hocr_props("name 1.5 2.5; name2 3.5 4.5"), expected);
}

TEST(Hocr, ParseSimpleFile)
{
    std::stringstream input(R"(
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
 <head>
  <title></title>
  <meta http-equiv="Content-Type" content="text/html;charset=utf-8"/>
  <meta name='ocr-system' content='tesseract 5.0.0-beta' />
  <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word ocrp_wconf ocrp_lang ocrp_dir ocrp_font ocrp_fsize'/>
 </head>
 <body>
  <div class='ocr_page' id='page_1' title='image "image.png"; bbox 0 0 1234 1234; ppageno 0; scan_res 144 144'>
   <div class='ocr_carea' id='block_1_1' title="bbox 22 4 634 28">
    <p class='ocr_par' id='par_1_1' lang='eng' title="bbox 22 4 634 28">
     <span class='ocr_line' id='line_1_1' title="bbox 22 4 634 28; baseline 0 -5; x_size 20; x_descenders 5; x_ascenders 4">
      <span class='ocrx_word' id='word_1_1' title='bbox 22 6 40 24; x_wconf 85; x_fsize 10'>
       <span class='ocrx_cinfo' title='x_bboxes 22 6 40 24; x_conf 97.936104'>X</span>
      </span>
      <span class='ocrx_word' id='word_1_2' title='bbox 51 9 141 23; x_wconf 91; x_fsize 10'>
       <span class='ocrx_cinfo' title='x_bboxes 51 9 64 23; x_conf 99.1'>a</span>
       <span class='ocrx_cinfo' title='x_bboxes 66 9 76 23; x_conf 98.2'>b</span>
       <span class='ocrx_cinfo' title='x_bboxes 77 9 88 23; x_conf 99.3'>c</span>
       <span class='ocrx_cinfo' title='x_bboxes 89 16 94 18; x_conf 99.4'>d</span>
       <span class='ocrx_cinfo' title='x_bboxes 96 9 107 23; x_conf 98.5'>e</span>
      </span>
      <span class='ocrx_word' id='word_1_3' title='bbox 149 8 257 28; x_wconf 92; x_fsize 10'>
       <span class='ocrx_cinfo' title='x_bboxes 149 12 159 28; x_conf 98.1'>i</span>
       <span class='ocrx_cinfo' title='x_bboxes 162 9 167 23; x_conf 98.2'>j</span>
       <span class='ocrx_cinfo' title='x_bboxes 172 9 182 23; x_conf 99.3'>k</span>
       <span class='ocrx_cinfo' title='x_bboxes 183 16 189 18; x_conf 98.4'>l</span>
      </span>
     </span>
    </p>
   </div>
  </div>
 </body>
</html>
)");

    std::vector<OcrParagraph> expected = {
        OcrParagraph{
            {
                OcrLine{
                    {
                        OcrWord{
                            {
                                OcrBox{22, 6, 40, 24},
                            },
                            OcrBox{22, 6, 40, 24},
                            85.0, -4.0, 0.0, 20.0,
                            "X"
                        },
                        OcrWord{
                            {
                                OcrBox{51, 9, 64, 23},
                                OcrBox{66, 9, 76, 23},
                                OcrBox{77, 9, 88, 23},
                                OcrBox{89, 16, 94, 18},
                                OcrBox{96, 9, 107, 23},
                            },
                            OcrBox{51, 9, 141, 23},
                            91.0, -5.0, 0.0, 20.0,
                            "abcde"
                        },
                        OcrWord{
                            {
                                OcrBox{149, 12, 159, 28},
                                OcrBox{162, 9, 167, 23},
                                OcrBox{172, 9, 182, 23},
                                OcrBox{183, 16, 189, 18},
                            },
                            OcrBox{149, 8, 257, 28},
                            92.0, 0.0, 0.0, 20.0,
                            "ijkl"
                        }
                    },
                    OcrBox{22, 4, 634, 28},
                    -5.0, 0.0
                }
            }
        }
    };

    ASSERT_EQ(read_hocr(input), expected);
}

} // namespace sanescan
