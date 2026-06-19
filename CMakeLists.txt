cmake_minimum_required(VERSION 3.24)
project(LumenForge
    VERSION 0.1.0
    DESCRIPTION "Offline-first native desktop photo editor"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.5 REQUIRED
    COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent)
qt_standard_project_setup(REQUIRES 6.5)
add_subdirectory(core)
add_subdirectory(app)
set(CPACK_PACKAGE_NAME        "LumenForge")
set(CPACK_PACKAGE_VENDOR      "LumenForge")
set(CPACK_PACKAGE_DESCRIPTION "")
set(CPACK_PACKAGE_VERSION     "")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    if(EXISTS "/app/resources/icon.ico")
        set(CPACK_WIX_PRODUCT_ICON "/app/resources/icon.ico")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
endif()
include(CPack)
