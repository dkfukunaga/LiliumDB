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

TEST(LRUPagerTest, NewPage) {
    std::string path = "test_db_file.db";
    {
        auto result = LRUPager::open(path, OpenMode::ReadWrite);
        ASSERT_TRUE(result.isOk());
        auto pager = std::move(result.value());
        ASSERT_TRUE(pager->isOpen());

        // append a Table page
        {
            auto result = pager->newPage(PageType::Table);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
        }

        // append an Index page
        {
            auto result = pager->newPage(PageType::Index);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
        }

        // append a FreeList page
        {
            auto result = pager->newPage(PageType::FreeList);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
        }
    }
    // pager goes out of scope and should close without error
}