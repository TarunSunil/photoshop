cmake_minimum_required(VERSION 3.16)
project(ai_core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(onnxruntime REQUIRED)

add_library(ai_runtime STATIC
    AiRuntime.hpp
    AiRuntime.cpp
    OnnxSession.hpp
    OnnxSession.cpp
    MaskPredictor.hpp
    MaskPredictor.cpp
    InpaintEngine.hpp
    InpaintEngine.cpp
    UpscaleEngine.hpp
    UpscaleEngine.cpp
)

target_include_directories(ai_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                              ${OpenCV_INCLUDE_DIRS})

target_link_libraries(ai_runtime PRIVATE onnxruntime::onnxruntime opencv_core opencv_imgproc)
