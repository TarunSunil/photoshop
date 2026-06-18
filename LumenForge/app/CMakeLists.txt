cmake_minimum_required(VERSION 3.16)
project(app VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

# Main application executable using windeployqt for deployment
add_executable(LumenForge 
    main.cpp
)

target_link_libraries(LumenForge PRIVATE shared_types mask_brush mask_document ai_runtime document_model image_render project_store Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Widgets Qt6::Concurrent)

# Link against required libraries for AI and image processing
find_package(OpenCV REQUIRED COMPONENTS imgproc imgcodecs contrib)
target_link_libraries(LumenForge PRIVATE OpenCV::opencv_world480 onnxruntime::onnxruntime ${LCMS_LIB})
