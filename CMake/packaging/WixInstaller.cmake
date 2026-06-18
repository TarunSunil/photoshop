# WiX Installer Generator for LumenForge
# This CMake module generates a Windows installer using WiX Toolset

include(CPack)

# Define the product name and version
set(PRODUCT_NAME "LumenForge")
set(PRODUCT_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")

# Include WiX generator
find_package(WindowsSDK REQUIRED)
find_program(WIX tool_prefix/wix.exe PATHS ${WIX_ROOT} $ENV{PATH})

if(NOT WIX)
    message(FATAL_ERROR "WiX Toolset not found. Please install from https://wixtoolset.org/")
endif()

# Configure WiX variables for installer generation
set(WIX_PRODUCT_UPGRADE_CODE "{A5C0F1D2-3E4B-5678-9ABC-DEF012345678}")
set(WIX_PACKAGE_NAME "${PRODUCT_NAME}-${CPACK_PACKAGE_VERSION}.exe")

# Generate the installer
install(CODE "message(STATUS \"Generating WiX Installer: ${WIX_PACKAGE_NAME}\")")
