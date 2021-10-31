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
#include <boost/math/constants/constants.hpp>
#include <gtest/gtest.h>

namespace sanescan {

namespace {
    const double deg_1 = 1.0 / 180 * boost::math::constants::pi<double>();
} // namespace

TEST(GetDominantAngle, NoValues)
{
    ASSERT_EQ(get_dominant_angle({}, 0), std::make_pair(0.0, 0.0));
}

TEST(GetDominantAngle, SingleValue)
{
    ASSERT_EQ(get_dominant_angle({{deg_1 * 10, 1}}, deg_1), std::make_pair(deg_1 * 10, 1.0));
    ASSERT_EQ(get_dominant_angle({{deg_1 * 350, 1}}, deg_1), std::make_pair(deg_1 * 350, 1.0));
    ASSERT_EQ(get_dominant_angle({{deg_1 * 360, 1}}, deg_1), std::make_pair(deg_1 * 360, 1.0));
}

TEST(GetDominantAngle, ManyValuesInSingleWindow)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 10, 1},
        {deg_1 * 11, 2},
        {deg_1 * 12, 3},
        {deg_1 * 13, 3},
        {deg_1 * 14, 2},
        {deg_1 * 15, 1},
    }, deg_1 * 10);

    EXPECT_NEAR(r.first, deg_1 * 12.5, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZero)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 358, 1},
        {deg_1 * 359, 2},
        {deg_1 * 360, 3},
        {deg_1 * 0, 3},
        {deg_1 * 1, 2},
        {deg_1 * 2, 1},
    }, deg_1 * 10);
    EXPECT_NEAR(r.first, 0.0, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZeroShiftedNeg)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 357, 1},
        {deg_1 * 358, 2},
        {deg_1 * 359, 3},
        {deg_1 * 360, 3},
        {deg_1 * 0, 2},
        {deg_1 * 1, 1},
    }, deg_1 * 10);
    EXPECT_NEAR(r.first, deg_1 * (-3*1 - 2*2 - 1*3 + 1*1) / 12, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInSingleWindowAcrossZeroShiftedPos)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 359, 1},
        {deg_1 * 360, 2},
        {deg_1 * 0, 3},
        {deg_1 * 1, 3},
        {deg_1 * 2, 2},
        {deg_1 * 3, 1},
    }, deg_1 * 10);
    EXPECT_NEAR(r.first, deg_1 * (-1*1 + 1*3 + 2*2 + 3*1) / 12, 1e-6);
    EXPECT_NEAR(r.second, 1.0, 1e-6);
}

TEST(GetDominantAngle, ManyValuesInMultipleWindows)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 10, 1},
        {deg_1 * 11, 2},
        {deg_1 * 12, 2},
        {deg_1 * 13, 3},
        {deg_1 * 14, 2},
        {deg_1 * 15, 1},
        {deg_1 * 30, 1},
        {deg_1 * 31, 2},
        {deg_1 * 32, 3},
        {deg_1 * 33, 3},
        {deg_1 * 34, 2},
        {deg_1 * 35, 1},
    }, deg_1 * 10);
    EXPECT_NEAR(r.first, deg_1 * 32.5, 1e-6);
    EXPECT_NEAR(r.second, 12.0 / 23.0, 1e-6);
}

TEST(GetDominantAngle, BetterValueShiftsOffWorseValues)
{
    auto r = get_dominant_angle(
    {
        {deg_1 * 10, 1},
        {deg_1 * 11, 1},
        {deg_1 * 12, 1},
        {deg_1 * 13, 1},
        {deg_1 * 14, 1},
        {deg_1 * 15, 1},
        {deg_1 * 16, 1},
        {deg_1 * 17, 1},
        {deg_1 * 18, 1},
        {deg_1 * 19, 1},
        {deg_1 * 20, 1},
        {deg_1 * 25, 10},
    }, deg_1 * 10);
    EXPECT_NEAR(r.first, deg_1 * (16 + 17 + 18 + 19 + 20 + 25*10) / 15.0, 1e-6);
    EXPECT_NEAR(r.second, 15.0 / 21.0, 1e-6);
}

} // namespace sanescan
