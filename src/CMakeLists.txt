
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 COMPONENTS Core Widgets)

set(SOURCES
    main.cc
    main_window.cc
    main_window.ui
)

add_executable(sanescan ${SOURCES})

target_link_libraries(sanescan
    Qt5::Core
    Qt5::Widgets
)
