Overview
========

**sanescan** is a scanning and OCR application that supports a variety of scanners via the SANE
scanner driver project.

The objective of sanescan is to produce pdf files with the best possible underlying text layer.
Text selection in document viewers should work as if the document was not scanned. The characters
in the text layer must correspond exactly to the underlying scanned image.

The application UI is currently beta quality.

Dependencies
============

Sanescan depends on the following libraries:

 - Qt
 - OpenCv
 - Tesseract 5 (for improved text selection in PDF documents, please take PRs 3599 and 3787
   from https://github.com/tesseract-ocr/tesseract/pulls)
 - boost
 - PoDoFo
 - poppler-cpp
 - pugixml
 - GTest

Building
========

sanescan uses CMake and ninja. To build, use the following instructions:

    mkdir build
    cd build
    cmake -G Ninja ..
    ninja -C .
    
Ninja is not mandatory, but other build systems such as make are not tested.

Acknowledgements
================

This project has received funding from NLnet foundation NGI0 Discovery Fund.

License
=======

The project is licensed under the General Public License, version 3 or (at your option) any later 
version. For the text of the license, see `LICENSE.gpl3` file.
