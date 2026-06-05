cmake_minimum_required(VERSION 3.24)

project(LumenForge
    VERSION 0.1.0
    DESCRIPTION "Offline-first native desktop photo editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(core)
add_subdirectory(app)
