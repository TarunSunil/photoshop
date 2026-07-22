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
        resources/qml/CropOverlay.qml
        resources/qml/LayerTransformOverlay.qml
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
target_include_directories(LumenForge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(LumenForge PROPERTIES
    WIN32_EXECUTABLE $<CONFIG:Release>
    MACOSX_BUNDLE TRUE
)
if(MSVC AND LUMEN_ENABLE_ONNX AND DEFINED onnxruntime_DIR)
    get_filename_component(ONNXRUNTIME_PREFIX "${onnxruntime_DIR}/../.." ABSOLUTE)
    set(ONNXRUNTIME_RUNTIME_DIR "${ONNXRUNTIME_PREFIX}/bin")

    if(NOT EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
        message(FATAL_ERROR "ONNX Runtime DLL not found: ${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
    endif()

    target_link_options(LumenForge PRIVATE /DELAYLOAD:onnxruntime.dll)
    target_link_libraries(LumenForge PRIVATE delayimp)

    # Qt's deployment step only copies Qt dependencies.  Copy all DLLs CMake
    # resolves for this executable as well, including ONNX Runtime and its
    # vcpkg-provided transitive dependencies.
    add_custom_command(TARGET LumenForge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:LumenForge>
            $<TARGET_FILE_DIR:LumenForge>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_shared.dll
            ${ONNXRUNTIME_RUNTIME_DIR}/re2.dll
            ${ONNXRUNTIME_RUNTIME_DIR}/libprotobuf.dll
            $<TARGET_FILE_DIR:LumenForge>
        COMMAND_EXPAND_LISTS
    )
endif()



install(TARGETS LumenForge BUNDLE DESTINATION . RUNTIME DESTINATION bin)
