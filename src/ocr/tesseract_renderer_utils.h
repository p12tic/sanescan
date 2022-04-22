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

#ifndef SANESCAN_OCR_TESSERACT_RENDERER_UTILS_H
#define SANESCAN_OCR_TESSERACT_RENDERER_UTILS_H

#include "ocr_baseline.h"
#include "ocr_box.h"
#include "util/math.h"
#include <tesseract/resultiterator.h>
#include <memory>

namespace sanescan {

inline OcrBox get_box_for_level(const std::unique_ptr<tesseract::ResultIterator>& it,
                                tesseract::PageIteratorLevel level)
{
    int left, top, right, bottom;
    it->BoundingBox(level, &left, &top, &right, &bottom);
    return OcrBox{left, top, right, bottom};
}

inline tesseract::Orientation get_orientation(const std::unique_ptr<tesseract::ResultIterator>& it)
{
    tesseract::Orientation orientation;
    tesseract::WritingDirection writing_direction;
    tesseract::TextlineOrder textline_order;
    float deskew_angle;
    it->Orientation(&orientation, &writing_direction, &textline_order,
                    &deskew_angle);
    return orientation;
}

inline OcrBaseline get_baseline(const std::unique_ptr<tesseract::ResultIterator>& it,
                                const OcrBox& box)
{
    int x1, y1, x2, y2;
    if (!it->Baseline(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2)) {
        return OcrBaseline{0, 0, 0};
    }

    double x1d = x1 - box.x1;
    double x2d = x2 - box.x1;
    double y1d = y1 - box.y2;
    double y2d = y2 - box.y2;

    if (x1d == x2d) {
        if (y1d > y2d) {
            return OcrBaseline{x1d, y1d, -deg_to_rad(90)};
        } else {
            return OcrBaseline{x1d, y1d, deg_to_rad(90)};
        }
    }

    double angle = std::atan((y2d - y1d) / (x2d - x1d));

    // The above method to compute the angle of the baseline always considers the baseline to go
    // from left to right. To properly compute baseline angle for upside-down text and similar
    // cases we need to look into the orientation.
    auto swap_angle_if_needed = [](double angle, double min_deg, double max_deg) {
        while (angle < deg_to_rad(min_deg)) {
            angle += deg_to_rad(180);
        }
        while (angle > deg_to_rad(max_deg)) {
            angle -= deg_to_rad(180);
        }
        return angle;
    };

    auto orientation = get_orientation(it);
    if (orientation == tesseract::ORIENTATION_PAGE_RIGHT) {
        // angle must be within 0 and 180 degrees
        angle = swap_angle_if_needed(angle, 0, 180);
    } else if (orientation == tesseract::ORIENTATION_PAGE_DOWN) {
        // angle must be within 90 and 270 degrees
        angle = swap_angle_if_needed(angle, 90, 270);
    } else if (orientation == tesseract::ORIENTATION_PAGE_LEFT) {
        // angle must be within 180 and 360 degrees
        angle = swap_angle_if_needed(angle, 180, 360);
    }

    return OcrBaseline{x1d, y1d, angle};
}

inline OcrBaseline adjust_baseline_for_other_box(const OcrBaseline& src_baseline,
                                                 const OcrBox& src_box, const OcrBox& dst_box)
{
    if (src_baseline.angle > deg_to_rad(45) || src_baseline.angle < -deg_to_rad(45)) {
        // baseline is more vertical than horizontal, adjust y baseline offset within
        // bounding box to zero
        auto y_diff = dst_box.y2 - (src_box.y2 + src_baseline.y);
        auto baseline_x_diff = -y_diff * std::tan(src_baseline.angle - deg_to_rad(90));
        auto x = src_box.x1 + src_baseline.x - dst_box.x1 + baseline_x_diff;
        return OcrBaseline{x, 0, src_baseline.angle};
    }

    // baseline is more horizontal than vertical, adjust x baseline offset within
    // bounding box to zero
    auto x_diff = dst_box.x1 - (src_box.x1 + src_baseline.x);
    auto baseline_y_diff = x_diff * std::tan(src_baseline.angle);
    auto y = src_box.y2 + src_baseline.y - dst_box.y2 + baseline_y_diff;
    return OcrBaseline{0, y, src_baseline.angle};
}

} // namespace sanescan

#endif // SANESCAN_OCR_TESSERACT_RENDERER_UTILS_H
