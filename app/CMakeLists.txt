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
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
install(TARGETS LumenForge
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
find_program(WINDEPLOYQT windeployqt
    HINTS "/../../../bin"
          "{QTDIR}/bin")
if(WINDEPLOYQT AND WIN32)
    add_custom_command(TARGET LumenForge POST_BUILD
        COMMAND 
            --qmldir "/resources/qml"
            $<TARGET_FILE:LumenForge>
        COMMENT "Running windeployqt…"
        VERBATIM)
endif()
