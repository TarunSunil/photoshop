cmake_minimum_required(VERSION 3.16)
project(storage VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Project store - persistence for layers, presets, settings
add_library(project_store STATIC
    ProjectStore.hpp
    ProjectStore.cpp
)

target_include_directories(project_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
