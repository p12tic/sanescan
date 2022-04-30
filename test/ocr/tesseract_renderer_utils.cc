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

#include "ocr/tesseract_renderer_utils.h"
#include <gtest/gtest.h>

namespace sanescan {

namespace {

bool ocr_baseline_near(const OcrBaseline& l, const OcrBaseline& r)
{
    double err = 1.0e-5;
    return std::abs(l.x - r.x) < err &&
            std::abs(l.y - r.y) < err &&
            std::abs(l.angle - r.angle) < err;
}

} // namespace

TEST(AdjustBaselineForOtherBox, Horizontal)
{
    ASSERT_EQ(adjust_baseline_for_other_box(OcrBaseline{0, 0, 0}, OcrBox{0, 0, 10, 20},
                                            OcrBox{10, 10, 30, 40}),
              (OcrBaseline{0, -20, 0}));
    ASSERT_EQ(adjust_baseline_for_other_box(OcrBaseline{10, 10, 0}, OcrBox{0, 0, 10, 20},
                                            OcrBox{10, 10, 30, 40}),
              (OcrBaseline{0, -10, 0}));
}

TEST(AdjustBaselineForOtherBox, ThirtyDegDown)
{
    ASSERT_TRUE(
        ocr_baseline_near(adjust_baseline_for_other_box(OcrBaseline{0, 0, deg_to_rad(-30)},
                                                        OcrBox{0, 0, 10, 20},
                                                        OcrBox{10, 10, 30, 40}),
                          OcrBaseline{0, -14.226497, deg_to_rad(-30)}));
}

TEST(AdjustBaselineForOtherBox, SixtyDegDown)
{
    ASSERT_TRUE(
        ocr_baseline_near(adjust_baseline_for_other_box(OcrBaseline{0, 0, deg_to_rad(-60)},
                                                        OcrBox{0, 0, 10, 20},
                                                        OcrBox{10, 10, 30, 40}),
                          OcrBaseline{1.547005, 0, deg_to_rad(-60)}));
}


TEST(AdjustBaselineForOtherBox, ThirtyDegUp)
{
    ASSERT_TRUE(
        ocr_baseline_near(adjust_baseline_for_other_box(OcrBaseline{0, 0, deg_to_rad(30)},
                                                        OcrBox{0, 0, 10, 20},
                                                        OcrBox{10, 10, 30, 40}),
                          OcrBaseline{0, -25.773503, deg_to_rad(30)}));
}

TEST(AdjustBaselineForOtherBox, SixtyDegUp)
{
    ASSERT_TRUE(
        ocr_baseline_near(adjust_baseline_for_other_box(OcrBaseline{0, 0, deg_to_rad(60)},
                                                        OcrBox{0, 0, 10, 20},
                                                        OcrBox{10, 10, 30, 40}),
                          OcrBaseline{-21.547005, 0, deg_to_rad(60)}));
}


} // namespace sanescan
