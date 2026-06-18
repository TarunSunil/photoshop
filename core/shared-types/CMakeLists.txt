add_library(shared_types STATIC
    Adjustment.hpp
)

target_include_directories(shared_types
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
