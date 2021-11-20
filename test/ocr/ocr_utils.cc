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

#include "ocr/ocr_utils.h"
#include "util/math.h"
#include <gtest/gtest.h>

namespace sanescan {

TEST(GetDominantAngle, NoValues)
{
    ASSERT_EQ(get_dominant_angle({}, deg_to_rad(360), 0), std::make_pair(0.0, 0.0));
}

TEST(GetDominantAngle, SingleValue)
{
    ASSERT_EQ(get_dominant_angle({{deg_to_rad(10), 1}}, deg_to_rad(360), deg_to_rad(1)),
              std::make_pair(deg_to_rad(10), 1.0));
    ASSERT_EQ(get_dominant_angle({{deg_to_rad(350), 1}}, deg_to_rad(360), deg_to_rad(1)),
              std::make_pair(deg_to_rad(350), 1.0));
    ASSERT_EQ(get_dominant_angle({{deg_to_rad(360), 1}}, deg_to_rad(360), deg_to_rad(1)),
              std::make_pair(deg_to_rad(0), 1.0));
}

TEST(GetDominantAngle, ManyValuesInSingleWindow)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(10), 1},
        {deg_to_rad(11), 2},
        {deg_to_rad(12), 3},
        {deg_to_rad(13), 3},
        {deg_to_rad(14), 2},
        {deg_to_rad(15), 1},
    }, deg_to_rad(360), deg_to_rad(10));

    EXPECT_NEAR(r.first, deg_to_rad(12.5), 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZero)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(358), 1},
        {deg_to_rad(359), 2},
        {deg_to_rad(360), 3},
        {deg_to_rad(0), 3},
        {deg_to_rad(1), 2},
        {deg_to_rad(2), 1},
    }, deg_to_rad(360), deg_to_rad(10));
    EXPECT_NEAR(r.first, 0.0, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZeroCustomWrapAround)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(498), 1},
        {deg_to_rad(499), 2},
        {deg_to_rad(500), 3},
        {deg_to_rad(0), 3},
        {deg_to_rad(1), 2},
        {deg_to_rad(2), 1},
    }, deg_to_rad(100), deg_to_rad(10));
    EXPECT_NEAR(r.first, 0.0, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZeroShiftedNeg)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(357), 1},
        {deg_to_rad(358), 2},
        {deg_to_rad(359), 3},
        {deg_to_rad(360), 3},
        {deg_to_rad(0), 2},
        {deg_to_rad(1), 1},
    }, deg_to_rad(360), deg_to_rad(10));
    EXPECT_NEAR(r.first, deg_to_rad((-3*1 - 2*2 - 1*3 + 1*1) / 12.0), 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZeroShiftedPos)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(359), 1},
        {deg_to_rad(360), 2},
        {deg_to_rad(0), 3},
        {deg_to_rad(1), 3},
        {deg_to_rad(2), 2},
        {deg_to_rad(3), 1},
    }, deg_to_rad(360), deg_to_rad(10));
    EXPECT_NEAR(r.first, deg_to_rad((-1*1 + 1*3 + 2*2 + 3*1) / 12.0), 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInMultipleWindows)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(10), 1},
        {deg_to_rad(11), 2},
        {deg_to_rad(12), 2},
        {deg_to_rad(13), 3},
        {deg_to_rad(14), 2},
        {deg_to_rad(15), 1},
        {deg_to_rad(30), 1},
        {deg_to_rad(31), 2},
        {deg_to_rad(32), 3},
        {deg_to_rad(33), 3},
        {deg_to_rad(34), 2},
        {deg_to_rad(35), 1},
    }, deg_to_rad(360), deg_to_rad(10));
    EXPECT_NEAR(r.first, deg_to_rad(32.5), 1e-6);
    EXPECT_NEAR(r.second, 12.0 / 23.0, 1e-6);
}

TEST(GetDominantAngle, BetterValueShiftsOffWorseValues)
{
    auto r = get_dominant_angle(
    {
        {deg_to_rad(10), 1},
        {deg_to_rad(11), 1},
        {deg_to_rad(12), 1},
        {deg_to_rad(13), 1},
        {deg_to_rad(14), 1},
        {deg_to_rad(15), 1},
        {deg_to_rad(16), 1},
        {deg_to_rad(17), 1},
        {deg_to_rad(18), 1},
        {deg_to_rad(19), 1},
        {deg_to_rad(20), 1},
        {deg_to_rad(25), 10},
    }, deg_to_rad(360), deg_to_rad(10));
    EXPECT_NEAR(r.first, deg_to_rad((16 + 17 + 18 + 19 + 20 + 25*10) / 15.0), 1e-6);
    EXPECT_NEAR(r.second, 15.0 / 21.0, 1e-6);
}

} // namespace sanescan
