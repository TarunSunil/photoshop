add_library(editor_core STATIC
    DocumentModel.cpp
    DocumentModel.hpp
)

target_include_directories(editor_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(editor_core
    PRIVATE
        shared_types::shared_types
        mask_core::mask_core
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
)
