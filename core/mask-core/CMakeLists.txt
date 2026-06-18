add_library(mask_core STATIC
    BrushEngine.cpp
    BrushEngine.hpp
    MaskDocument.cpp
    MaskDocument.hpp
)

target_include_directories(mask_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mask_core
    PRIVATE
        shared_types::shared_types
        Qt6::Core
        Qt6::Gui
)
