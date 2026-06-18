cmake_minimum_required(VERSION 3.24)

project(LumenForge
    VERSION 0.1.0
    DESCRIPTION "Offline-first native desktop photo editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(core)
add_subdirectory(app)

# Set output directories for generated files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# CPack configuration for installer generation
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VENDOR "LumenForge Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

# WiX/NSIS installer generator configuration
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME} Installer")
set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")

# Create installer only if CPack is configured
if(DEFINED CPACK_GENERATOR)
    include(CPack)
endif()
