cmake_minimum_required(VERSION 3.16)
project(LumenForge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Quick Qml Widgets Concurrent)

# Add subdirectories in order of dependencies
add_subdirectory(core/shared-types)
add_subdirectory(core/mask-core)
add_subdirectory(core/ai-core)
add_subdirectory(core/editor-core)
add_subdirectory(core/image-core)
add_subdirectory(core/export-core)
add_subdirectory(core/storage)
add_subdirectory(app)

# Install rules using CPack with WiX generator
include(CPackConfig)
