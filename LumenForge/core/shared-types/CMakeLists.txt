cmake_minimum_required(VERSION 3.16)
project(shared_types VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Add shared type headers (no implementation files yet - just declarations)
add_library(shared_types STATIC
    Adjustment.hpp
    Layer.hpp
    Mask.hpp
)

target_include_directories(shared_types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
