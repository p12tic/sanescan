cmake_minimum_required(VERSION 3.16.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SANESCAN_VERSION 0.0.1)

project(sanescan VERSION ${SANESCAN_VERSION})

include_directories("${CMAKE_SOURCE_DIR}/src")
add_subdirectory(src)
add_subdirectory(test)
