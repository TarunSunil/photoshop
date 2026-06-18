cmake_minimum_required(VERSION 3.16)
project(core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Add subdirectories in dependency order
add_subdirectory(shared-types)
add_subdirectory(mask-core)
add_subdirectory(ai-core)
add_subdirectory(editor-core)
add_subdirectory(image-core)
add_subdirectory(storage)
