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

#include "ocr/tesseract.h"
#include "ocr/ocr_utils.h"
#include "ocr/ocr_options.h"
#include "util/math.h"
#include "ocr/pdf.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>
#include <string>

namespace sanescan {

bool read_ocr_write(const std::string& input_path, const std::string& output_path,
                    bool debug_ocr, OcrOptions options)
{
    auto image = cv::imread(input_path);
    if (image.data == nullptr) {
        throw std::runtime_error("Could not load input file");
    }

    TesseractRecognizer recognizer{"/usr/share/tesseract-ocr/4.00/tessdata/"};

    auto [adjusted_image, recognized] = recognizer.recognize(image, options);

    sanescan::OcrParagraph combined;
    for (const auto& par : recognized) {
        for (const auto& line : par.lines) {
            combined.lines.push_back(line);
        }
    }

    std::vector<OcrParagraph> sorted_all = {sort_paragraph_text(combined)};
    std::ofstream stream_pdf(output_path);
    write_pdf(stream_pdf, adjusted_image, sorted_all,
              debug_ocr ? WritePdfFlags::DEBUG_CHAR_BOXES : WritePdfFlags::NONE);
    return true;
}

} // namespace sanescan

struct Options {
    static constexpr const char* INPUT_PATH = "input-path";
    static constexpr const char* OUTPUT_PATH = "output-path";
    static constexpr const char* HELP = "help";
    static constexpr const char* DEBUG = "debug";

    static constexpr const char* FIX_ROTATION_ENABLE = "ocr-enable-fix-text-rotation";
    static constexpr const char* FIX_ROTATION_FRACTION = "ocr-fix-text-rotation-min-text-fraction";
    static constexpr const char* FIX_ROTATION_ANGLE = "ocr-fix-text-rotation-max-angle-diff";

    static constexpr const char* FIX_ORIENTATION_ENABLE = "ocr-enable-fix-page-orientation";
    static constexpr const char* FIX_ORIENTATION_FRACTION = "ocr-fix-page-orientation-min-text-fraction";
    static constexpr const char* FIX_ORIENTATION_ANGLE = "ocr-fix-page-orientation-max-angle-diff";
};

int main(int argc, char* argv[])
{
    namespace po = boost::program_options;

    std::string input_path;
    std::string output_path;

    po::positional_options_description positional_options_desc;
    positional_options_desc.add(Options::INPUT_PATH, 1);
    positional_options_desc.add(Options::OUTPUT_PATH, 1);

    auto introduction_desc = R"(Usage:
    sanescancli [OPTION]... [input_path] [output_path]

input_path and output_path options can be passed either as positional or named arguments.
)";

    po::options_description options_desc("Options");

    options_desc.add_options()
            (Options::INPUT_PATH, po::value(&input_path), "the path to the input image")
            (Options::OUTPUT_PATH, po::value(&output_path), "the path to the output PDF file")
            (Options::HELP, "produce this help message")
            (Options::DEBUG, "enable debugging output in the output PDF file");

    sanescan::OcrOptions ocr_options;

    po::options_description ocr_options_desc("OCR options");

    ocr_options_desc.add_options()
            (Options::FIX_ROTATION_ENABLE,
             "enable adjusting image rotation to make text lines level")
            (Options::FIX_ROTATION_FRACTION,
             po::value(&ocr_options.fix_text_rotation_min_text_fraction)->default_value(0.95, "0.95"),
             "minimum fraction of the text characters pointing to the same direction (modulo 90 "
             "degrees) to consider image rotation")
            (Options::FIX_ROTATION_ANGLE,
             po::value(&ocr_options.fix_text_rotation_max_angle_diff)->default_value(5),
             "maximum difference between the text direction and any level direction in degrees to "
             "consider image rotation")

            (Options::FIX_ORIENTATION_ENABLE,
             "enable automatic fixing of page orientation")
            (Options::FIX_ORIENTATION_FRACTION,
             po::value(&ocr_options.fix_page_orientation_min_text_fraction)->default_value(0.95, "0.95"),
             "minimum fraction of the text characters pointing to the same direction to consider "
             "page orientation")
            (Options::FIX_ORIENTATION_FRACTION,
             po::value(&ocr_options.fix_page_orientation_max_angle_diff)->default_value(5),
             "maximum difference between the text direction and any level direction in degrees to "
             "consider page orientation fix")
    ;

    po::variables_map options;
    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(options_desc)
                      .positional(positional_options_desc)
                      .run(),
                  options);
        po::notify(options);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse options: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Failed to parse options: unknown error\n";
        return EXIT_FAILURE;
    }

    if (options.count(Options::HELP)) {
        std::cout << introduction_desc << "\n"
                  << options_desc << "\n"
                  << ocr_options_desc << "\n";
        return EXIT_SUCCESS;
    }

    if (options.count(Options::INPUT_PATH) != 1) {
        std::cerr << "Must specify single input path\n";
        return EXIT_FAILURE;
    }

    if (options.count(Options::OUTPUT_PATH) != 1) {
        std::cerr << "Must specify single output path\n";
        return EXIT_FAILURE;
    }

    if (!options.count(Options::FIX_ROTATION_ENABLE)) {
        if (options.count(Options::FIX_ROTATION_FRACTION)) {
            std::cerr << "Can't specify " << Options::FIX_ROTATION_FRACTION << " without "
                      << Options::FIX_ROTATION_ENABLE << "\n";
            return EXIT_FAILURE;
        }

        if (options.count(Options::FIX_ROTATION_ANGLE)) {
            std::cerr << "Can't specify " << Options::FIX_ROTATION_ANGLE << " without "
                      << Options::FIX_ROTATION_ENABLE << "\n";
            return EXIT_FAILURE;
        }
    }

    if (!options.count(Options::FIX_ORIENTATION_ENABLE)) {
        if (options.count(Options::FIX_ORIENTATION_FRACTION)) {
            std::cerr << "Can't specify " << Options::FIX_ORIENTATION_FRACTION << " without "
                      << Options::FIX_ORIENTATION_ENABLE << "\n";
            return EXIT_FAILURE;
        }

        if (options.count(Options::FIX_ORIENTATION_ANGLE)) {
            std::cerr << "Can't specify " << Options::FIX_ORIENTATION_ANGLE << " without "
                      << Options::FIX_ORIENTATION_ENABLE << "\n";
            return EXIT_FAILURE;
        }
    }

    ocr_options.fix_text_rotation = options.count(Options::FIX_ROTATION_ENABLE);
    ocr_options.fix_page_orientation = options.count(Options::FIX_ORIENTATION_ENABLE);
    ocr_options.fix_page_orientation_max_angle_diff =
            sanescan::deg_to_rad(ocr_options.fix_page_orientation_max_angle_diff);
    ocr_options.fix_text_rotation_max_angle_diff =
            sanescan::deg_to_rad(ocr_options.fix_text_rotation_max_angle_diff);

    try {
        if (!sanescan::read_ocr_write(input_path, output_path,
                                      options.count(Options::DEBUG), ocr_options)) {
            std::cerr << "Unknown failure";
            return EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to do OCR: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Failed to do OCR uknown failure\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
