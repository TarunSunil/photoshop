cmake_minimum_required(VERSION 3.16)
project(editor_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Document model - manages layers and document state
add_library(document_model STATIC
    DocumentModel.hpp
    DocumentModel.cpp
)

target_include_directories(document_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
