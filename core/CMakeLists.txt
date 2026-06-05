add_library(lumen_core STATIC
    shared-types/Adjustment.hpp
    shared-types/Layer.hpp
    shared-types/Mask.hpp
    editor-core/DocumentModel.cpp
    editor-core/DocumentModel.hpp
    image-core/RenderPipeline.cpp
    image-core/RenderPipeline.hpp
    mask-core/MaskDocument.cpp
    mask-core/MaskDocument.hpp
    ai-core/AiRuntime.cpp
    ai-core/AiRuntime.hpp
    export-core/ExportService.cpp
    export-core/ExportService.hpp
    storage/ProjectStore.cpp
    storage/ProjectStore.hpp
)

target_include_directories(lumen_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lumen_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Concurrent
)
