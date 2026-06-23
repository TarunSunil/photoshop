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
# Delay-load onnxruntime.dll so it is NOT pulled into the process import table
# at startup.  Without this, Windows must resolve onnxruntime.dll and every
# transitive dependency (abseil, protobuf, re2 …) the moment LumenForge.exe
# launches.  If any one of those DLLs is absent the process silently terminates
# before main() even runs.  With delay-load, the DLL is loaded the first time
# an ORT API is actually called (i.e. when the user invokes an AI feature),
# and any loading failure surfaces as a catchable exception there rather than a
# silent startup death.
if(MSVC)
    target_link_options(LumenForge PRIVATE /DELAYLOAD:onnxruntime.dll)
    target_link_libraries(LumenForge PRIVATE delayimp)
endif()
install(TARGETS LumenForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
