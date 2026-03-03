include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(byte_span_test byte_span_test.cpp)
target_link_libraries(byte_span_test PRIVATE liliumdb_headers GTest::gtest_main)
gtest_discover_tests(byte_span_test)

add_executable(status_test status_test.cpp)
target_link_libraries(status_test PRIVATE liliumdb_headers GTest::gtest_main)
gtest_discover_tests(status_test)

add_executable(std_page_io_test std_page_io_test.cpp)
target_link_libraries(std_page_io_test PRIVATE liliumdb_lib GTest::gtest_main)
gtest_discover_tests(std_page_io_test)