cmake_minimum_required(VERSION 3.16)

project(one VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC_COMPILER_PREDEFINES OFF)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Network)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appone
    main.cpp
    appcontroller.cpp
    controlmodel.cpp
)

target_sources(appone
    PRIVATE
        appcontroller.h
        controlmodel.h
)

qt_add_qml_module(appone
    URI one
    VERSION 1.0
    QML_FILES
        Main.qml
)

set_target_properties(appone PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appone
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
        Qt6::Network
)

install(TARGETS appone
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
