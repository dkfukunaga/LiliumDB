#include "pager/lru_pager.h"

#include <filesystem>

#include "gtest/gtest.h"

using namespace LiliumDB;

class LRUPagerTest : public ::testing::Test {
protected:
    std::string path = "test_db_file.db";
    std::unique_ptr<Pager> pager = nullptr;

    void SetUp() override {
        if (std::filesystem::exists((path))) {
            std::filesystem::remove(path);
        }
    }

    void TearDown() override {
        
    }
};

TEST_F(LRUPagerTest, OpenClose) {
    {
        auto result = LRUPager::open(path, OpenMode::ReadWrite);
        ASSERT_TRUE(result.isOk());
        pager = std::move(result.value());
        ASSERT_TRUE(pager->isOpen());
    }
    // pager goes out of scope and should close without error
}

TEST_F(LRUPagerTest, NewPage) {
    {
        auto result = LRUPager::open(path, OpenMode::ReadWrite);
        ASSERT_TRUE(result.isOk());
        pager = std::move(result.value());
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

        // append 5 Table pages
        for (int i = 0; i < 5; ++i) {
            auto result = pager->newPage(PageType::Table);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
        }
    }
    // pager goes out of scope and should close without error
}