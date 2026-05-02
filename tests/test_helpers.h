#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "pager/pager.h"
#include "pager/page_guard.h"
#include "btree/btree_page.h"
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
    // printf("project root: %s\n", testDirectory.c_str());

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

        LiliumDB::hexdump(hexdumpFile, page.view(), i * LiliumDB::PAGE_SIZE, "Page " + std::to_string(i));
    }
}

void logPageInfo(LiliumDB::Pager& pager,
    std::string_view testFileName,
    const std::string& insertKey,
    int count) {
    EXPECT_TRUE(pager.flushAll());

    std::string projectRoot = PROJECT_ROOT_STR;
    std::string testSuite = ::testing::UnitTest::GetInstance()->current_test_info()->test_suite_name();
    std::string now = std::to_string(std::time(nullptr));
    std::string testDirectory = projectRoot + "/debug/" + testSuite + "/page_log" + now + "/";

    std::filesystem::create_directories(testDirectory);

    std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    std::string logPath = testDirectory + "/" + testName + "_" + std::to_string(count) + "_" + insertKey + ".log";

    std::ofstream logFile(logPath);

    logFile << testFileName << "\n" << "inserting: " << insertKey << "\n" << pager.pageCount() << " pages\n\n";
    
    for (int i = 0; i < pager.pageCount(); ++i) {
        auto r = pager.fetchPage(i);
        if (!r) continue;
        auto page = std::move(r.value());
        auto header = page.getHeader();
        
        logFile << "Page " << i << ":\n";
        logFile << "  level: " << (int)header.level << "\n";
        logFile << "  count: " << header.slotCount << "\n";
        logFile << "  slots:\n";
        
        for (int s = 0; s < header.slotCount; ++s) {
            auto slot = page.view().get<LiliumDB::Slot>(LiliumDB::BTreePage::slotOffset(s));
            logFile << "    " << s << ": offset=" << slot.offset << " size=" << slot.size << "\n";
        }
        
        logFile << "  keys:\n";
        for (int s = 0; s < header.slotCount; ++s) {
            auto slot = page.view().get<LiliumDB::Slot>(LiliumDB::BTreePage::slotOffset(s));
            if (header.level > 0) {
                auto kh = page.view().get<LiliumDB::KeyHeader>(slot.offset);
                auto keyData = (char*)page.view().data() + slot.offset + sizeof(LiliumDB::KeyHeader);
                logFile << "    " << std::string(keyData, 2) << " -> page " << kh.childPage << "\n";
            } else {
                auto kv = page.view().get<LiliumDB::KeyValueHeader>(slot.offset);
                auto keyData = (char*)page.view().data() + slot.offset + sizeof(LiliumDB::KeyValueHeader);
                logFile << "    " << std::string(keyData, 2) << "\n";
            }
        }
        if (header.level > 0) {
            logFile << "  next -> page " << header.next << "\n";
        } else {
            logFile << "  next -> page " << header.next << "\n";
            logFile << "  prev -> page " << header.prev << "\n";
        }
        logFile << "\n";
    }
}

} // anonymous namespace

#endif