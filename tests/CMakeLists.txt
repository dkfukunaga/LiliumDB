include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# add_executable(std_page_io_test std_page_io_test.cpp)
# target_link_libraries(std_page_io_test PRIVATE liliumdb_lib GTest::gtest_main)
# gtest_discover_tests(std_page_io_test)