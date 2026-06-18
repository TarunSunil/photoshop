add_library(storage STATIC
    ProjectStore.cpp
    ProjectStore.hpp
)

target_include_directories(storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find SQLite for project storage
find_package(SQLite3 REQUIRED)

target_link_libraries(storage
    PRIVATE
        shared_types::shared_types
        editor_core::editor_core
        sqlite3::sqlite3
        Qt6::Core
        Qt6::Sql
)
