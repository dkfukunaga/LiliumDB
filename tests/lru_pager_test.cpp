#include "pager/lru_pager.h"

#include "gtest/gtest.h"

using namespace LiliumDB;

TEST(LRUPagerTest, OpenClose) {
    std::string path = "test_db_file.db";
    {
        auto result = LRUPager::open(path, OpenMode::ReadWrite);
        ASSERT_TRUE(result.isOk());
        auto pager = std::move(result.value());
        ASSERT_TRUE(pager->isOpen());
    }
    // pager goes out of scope and should close without error
}