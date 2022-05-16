Overview
========

**sanescan** is a scanning and OCR application that supports a variety of scanners via the SANE
scanner driver project.

The application UI is currently beta quality.

Building
========

sanescan uses CMake and ninja. To build, use the following instructions:

    mkdir build
    cd build
    cmake -G Ninja ..
    ninja -C .
    
Ninja is not mandatory, but other build systems such as make are not tested.

License
=======

The project is licensed under the General Public License, version 3 or (at your option) any later 
version. For the text of the license, see `LICENSE.gpl3` file.
