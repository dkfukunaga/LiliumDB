add_library(liliumdb_lib STATIC
    storage/std_page_io.cpp
)

target_include_directories(liliumdb_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_compile_options(liliumdb_lib PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow
    -Wformat=2 -Wcast-align -Wconversion
    -Wsign-conversion -Wnull-dereference
)