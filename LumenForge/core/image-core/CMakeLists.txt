cmake_minimum_required(VERSION 3.16)
project(image_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Find required packages for image processing
find_package(OpenCV REQUIRED COMPONENTS imgproc imgcodecs)
find_package(lcms2 REQUIRED)

add_library(image_render STATIC
    RenderPipeline.hpp
    RenderPipeline.cpp
    BlendModes.hpp
    BlendModes.cpp
    RawImporter.hpp
    RawImporter.cpp
    ColorManager.hpp
    ColorManager.cpp
)

target_include_directories(image_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                                ${OpenCV_INCLUDE_DIRS})

# Link against Little CMS for color management
find_library(LCMS_LIB lcms2 REQUIRED PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
target_link_libraries(image_render PRIVATE ${LCMS_LIB})
