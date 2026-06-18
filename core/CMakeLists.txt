cmake_minimum_required(VERSION 3.24)

project(LumenForgeCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Sql Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

# Add subdirectories in dependency order
add_subdirectory(shared-types)
add_subdirectory(mask-core)
add_subdirectory(editor-core)
add_subdirectory(export-core)
add_subdirectory(image-core)
add_subdirectory(ai-core)
add_subdirectory(storage)
