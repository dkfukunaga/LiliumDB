add_library(liliumdb_headers INTERFACE)
target_include_directories(liliumdb_headers INTERFACE ${PROJECT_SOURCE_DIR}/src)

add_library(liliumdb_lib STATIC)
target_link_libraries(liliumdb_lib PUBLIC liliumdb_headers)

target_compile_options(liliumdb_lib PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow
    -Wformat=2 -Wcast-align -Wconversion
    -Wsign-conversion -Wnull-dereference
)

add_subdirectory(storage)
add_subdirectory(pager)
add_subdirectory(utils)