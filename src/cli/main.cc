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

#include <boost/program_options.hpp>
#include <string>
#include <iostream>

namespace po = boost::program_options;

int main(int argc, char* argv[])
{
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

    return EXIT_SUCCESS;
}
