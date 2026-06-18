add_library(image_core STATIC
    RenderPipeline.cpp
    RenderPipeline.hpp
)

target_include_directories(image_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find required libraries for image processing
find_package(OpenCV 4.5 REQUIRED COMPONENTS imgcodecs opencv_imgproc opencv_highgui)
find_package(lcms2 REQUIRED)
find_package(LibRaw REQUIRED)

target_link_libraries(image_core
    PRIVATE
        shared_types::shared_types
        editor_core::editor_core
        OpenCV::imgcodecs
        OpenCV::opencv_imgproc
        OpenCV::opencv_highgui
        lcms2::lcms2
        LibRaw::LibRaw
        Qt6::Core
        Qt6::Gui
)

# Create additional libraries for advanced features (optional, can be disabled with -DIMAGE_CORE_FEATURES=OFF)
option(ENABLE_BLEND_MODES "Enable blend modes support" ON)
if(ENABLE_BLEND_MODES)
    add_library(blend_modes STATIC
        BlendModes.cpp
        BlendModes.hpp
    )

    target_include_directories(blend_modes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(blend_modes PRIVATE image_core::image_core Qt6::Core)
endif()

option(ENABLE_RAW_IMPORTER "Enable LibRaw raw file importer" ON)
if(ENABLE_RAW_IMPORTER)
    add_library(raw_importer STATIC
        RawImporter.cpp
        RawImporter.hpp
    )

    target_include_directories(raw_importer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(raw_importer PRIVATE image_core::image_core LibRaw::LibRaw Qt6::Core)
endif()

option(ENABLE_COLOR_MANAGER "Enable Little CMS color management" ON)
if(ENABLE_COLOR_MANAGER)
    add_library(color_manager STATIC
        ColorManager.cpp
        ColorManager.hpp
    )

    target_include_directories(color_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(color_manager PRIVATE image_core::image_core lcms2::lcms2 Qt6::Core)
endif()
