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
#include "util/math.h"
#include "ocr/pdf.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>
#include <string>

namespace sanescan {

void rotate_image(cv::Mat& image, double angle_rad)
{
    auto height = image.size.p[0];
    auto width = image.size.p[1];

    cv::Mat rotation_mat = cv::getRotationMatrix2D(cv::Point2f(width / 2, height / 2),
                                                   rad_to_deg(angle_rad), 1.0);

    cv::Mat rotated_image;
    cv::warpAffine(image, rotated_image, rotation_mat, image.size(),
                   cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    image = rotated_image;
}

void prepare_image_rotation(TesseractRecognizer& recognizer,
                            cv::Mat& image, std::vector<OcrParagraph>& recognized)
{
    // Handle the case when all text within the image is rotated slightly due to the input data
    // scan just being rotated. In such case whole image will be rotated to address the following
    // issues:
    //
    // - Most PDF readers can't select rotated text properly
    // - The OCR accuracy is compromised for rotated text.
    //
    // TODO: Ideally we should detect cases when the text in the source image is legitimately
    // rotated and the rotation is not just the artifact of rotation. In such case the accuracy of
    // OCR will still be improved if rotate the source image just for OCR and then rotate the
    // results back.
    //
    // While handling the slightly rotated text case we can also detect whether the page is rotated
    // 90, 180 or 270 degrees. We rotate it back so that the text is horizontal which helps text
    // selection in PDF readers.
    auto all_text_angles = get_all_text_angles(recognized);

    {
        auto [angle, in_window] = get_dominant_angle(all_text_angles,
                                                     deg_to_rad(360), deg_to_rad(5));
        double angle_mod90 = near_zero_fmod(angle, deg_to_rad(90));
        if (std::abs(angle_mod90) < deg_to_rad(5) && in_window > 0.95) {

            // In this case we want to rotate whole page which changes the dimensions of the image.
            // First we use cv::rotate to rotate 90, 180 or 270 degrees and then rotate_image
            // for the final adjustment.

            // Use approximate comparison so that computation accuracy does not affect the
            // comparison results.
            double eps = 0.1;
            if (angle - angle_mod90 > deg_to_rad(270 - eps)) {
                angle -= deg_to_rad(270);
                cv::rotate(image, image, cv::ROTATE_90_CLOCKWISE);
            } else if (angle - angle_mod90 > deg_to_rad(180 - eps)) {
                angle -= deg_to_rad(180);
                cv::rotate(image, image, cv::ROTATE_180);
            } else if (angle - angle_mod90 > deg_to_rad(90 - eps)) {
                angle -= deg_to_rad(90);
                cv::rotate(image, image, cv::ROTATE_90_COUNTERCLOCKWISE);
            }

            rotate_image(image, angle);
            recognized = recognizer.recognize(image);
            return;
        }
    }

    {
        auto [angle, in_window] = get_dominant_angle(all_text_angles,
                                                     deg_to_rad(90), deg_to_rad(5));
        if (std::abs(angle) < deg_to_rad(5) && in_window > 0.95) {
            rotate_image(image, angle);
            recognized = recognizer.recognize(image);
            return;
        }
    }
}

bool read_ocr_write(const std::string& input_path, const std::string& output_path,
                    bool debug_ocr)
{
    auto image = cv::imread(input_path);
    if (image.data == nullptr) {
        throw std::runtime_error("Could not load input file");
    }

    TesseractRecognizer recognizer{"/usr/share/tesseract-ocr/4.00/tessdata/"};

    auto recognized = recognizer.recognize(image);

    prepare_image_rotation(recognizer, image, recognized);

    sanescan::OcrParagraph combined;
    for (const auto& par : recognized) {
        for (const auto& line : par.lines) {
            combined.lines.push_back(line);
        }
    }

    std::vector<OcrParagraph> sorted_all = {sort_paragraph_text(combined)};
    std::ofstream stream_pdf(output_path);
    write_pdf(stream_pdf, image, sorted_all,
              debug_ocr ? WritePdfFlags::DEBUG_CHAR_BOXES : WritePdfFlags::NONE);
    return true;
}

} // namespace sanescan

int main(int argc, char* argv[])
{
    namespace po = boost::program_options;

    std::string input_path;
    std::string output_path;

    po::positional_options_description positional_options_desc;
    positional_options_desc.add("input-path", 1);
    positional_options_desc.add("output-path", 1);

    po::options_description options_desc(R"(Usage:
    sanescancli [OPTION]... [input_path] [output_path]

input_path and output_path options can be passed either as positional or named arguments.

Options)");

    options_desc.add_options()
            ("input-path", po::value(&input_path), "the path to the input image")
            ("output-path", po::value(&output_path), "the path to the output PDF file")
            ("help", "produce this help message")
            ("debug", "enable debugging output in the output PDF file");

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

    if (options.count("help")) {
        std::cout << options_desc << "\n";
        return EXIT_SUCCESS;
    }

    if (options.count("input-path") != 1) {
        std::cerr << "Must specify single input path\n";
        return EXIT_FAILURE;
    }

    if (options.count("output-path") != 1) {
        std::cerr << "Must specify single output path\n";
        return EXIT_FAILURE;
    }

    try {
        if (!sanescan::read_ocr_write(input_path, output_path, options.count("debug"))) {
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
