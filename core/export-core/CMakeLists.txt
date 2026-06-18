add_library(export_core STATIC
    ExportService.cpp
    ExportService.hpp
)

target_include_directories(export_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(export_core
    PRIVATE
        shared_types::shared_types
        editor_core::editor_core
        Qt6::Core
        Qt6::Concurrent
)
