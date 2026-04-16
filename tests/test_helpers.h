#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "pager/pager.h"
#include "pager/page_guard.h"
#include "utils/hexdump.h"

#include <string_view>
#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"

// PROJECT_ROOT is defined by CMake as an unquoted token (e.g. -DPROJECT_ROOT=C:/foo).
// Two-level stringify is required: the outer STRINGIFY macro forces expansion of
// PROJECT_ROOT before the inner STRINGIFY_IMPL applies #, which converts the
// expanded token sequence to a string literal. Without the indirection, # would
// stringify the literal token "PROJECT_ROOT" instead of its expanded value.
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define PROJECT_ROOT_STR STRINGIFY(PROJECT_ROOT)

namespace {

void testHexDump(LiliumDB::Pager& pager, std::string_view testFileName) {
    EXPECT_TRUE(pager.flushAll());

    std::string projectRoot = PROJECT_ROOT_STR;
    std::string testSuite = ::testing::UnitTest::GetInstance()->current_test_info()->test_suite_name();
    std::string testDirectory = projectRoot + "/debug/" + testSuite;
    printf("project root: %s\n", testDirectory.c_str());

    std::filesystem::create_directories(testDirectory);

    std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::string now = std::to_string(std::time(nullptr));
    std::string hexdumpPath = testDirectory + "/" + testName + "_" + now + ".hex";

    std::ofstream hexdumpFile(hexdumpPath);

    hexdumpFile << testFileName << "\n" << pager.pageCount() << " pages\n\n";

    for (LiliumDB::PageNum i = 0; i < pager.pageCount(); ++i) {
        auto r = pager.fetchPage(i);
        EXPECT_TRUE(r);
        auto page = std::move(r.value());

        hexdump(hexdumpFile, page.view(), i * LiliumDB::PAGE_SIZE, "Page " + std::to_string(i));
    }
}

} // anonymous namespace

#endif