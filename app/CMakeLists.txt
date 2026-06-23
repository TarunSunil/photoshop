qt_add_executable(LumenForge
    src/main/main.cpp
    src/editor/DocumentController.cpp
    src/editor/DocumentController.hpp
)
qt_add_qml_module(LumenForge
    URI LumenForge
    VERSION 1.0
    QML_FILES
        resources/qml/Main.qml
        resources/qml/MaskCanvas.qml
)
target_link_libraries(LumenForge
    PRIVATE
        lumen_core
        Qt6::Core
        Qt6::Gui
        Qt6::Concurrent
        Qt6::Quick
        Qt6::QuickControls2
)
target_include_directories(LumenForge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(LumenForge PROPERTIES
    # Release: proper WIN32 GUI exe (no console window).
    # Debug: console exe so qDebug/qCritical/stdout are visible in the terminal.
    WIN32_EXECUTABLE $<CONFIG:Release>
    MACOSX_BUNDLE TRUE
)
# Delay-load onnxruntime.dll — only relevant when ONNX is actually linked.
# When LUMEN_ENABLE_ONNX=ON, onnxruntime.dll must NOT be pulled into the
# process import table at startup because onnxruntime_cxx_api.h has a global
# static initialiser that calls OrtGetApiBase() before main() runs (see
# core/CMakeLists.txt for the full explanation).  DELAYLOAD defers the DLL
# load until the first ORT API call inside the event loop, where we have
# proper exception handling.
if(MSVC AND LUMEN_ENABLE_ONNX)
    target_link_options(LumenForge PRIVATE /DELAYLOAD:onnxruntime.dll)
    target_link_libraries(LumenForge PRIVATE delayimp)
endif()
install(TARGETS LumenForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
