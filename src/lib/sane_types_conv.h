/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Monika Kairaityte <monika@kibit.lt>

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

#ifndef SANESCAN_LIB_SANE_TYPES_CONV_H
#define SANESCAN_LIB_SANE_TYPES_CONV_H

#include "sane_types.h"
#include <sane/sane.h>

namespace sanescan {

inline SaneUnit sane_unit_to_sanescan(SANE_Unit unit)
{
    return static_cast<SaneUnit>(unit);
}

inline SaneValueType sane_value_type_to_sanescan(SANE_Value_Type type)
{
    return static_cast<SaneValueType>(type);
}

inline SaneCap sane_cap_to_sanescan(SANE_Int cap)
{
    return static_cast<SaneCap>(cap);
}

inline SaneFrameType sane_frame_type_to_sanescan(SANE_Frame frame)
{
    return static_cast<SaneFrameType>(frame);
}

inline SaneOptionSetInfo sane_options_info_to_sanescan(SANE_Int info)
{
    return static_cast<SaneOptionSetInfo>(info);
}

} // namespace sanescan

#endif
