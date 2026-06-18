cmake_minimum_required(VERSION 3.16)
project(mask_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Brush engine - brush and erase tools
add_library(mask_brush STATIC
    BrushEngine.hpp
    BrushEngine.cpp
)

target_include_directories(mask_brush PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Mask document - mask management
add_library(mask_document STATIC
    MaskDocument.hpp
    MaskDocument.cpp
)

target_link_libraries(mask_document PRIVATE shared_types)
target_include_directories(mask_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
