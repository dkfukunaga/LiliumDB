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

TEST_F(LRUPagerTest, NewPagefetchPage) {
    {
        auto result = LRUPager::open(path, OpenMode::ReadWrite);
        ASSERT_TRUE(result.isOk());
        pager = std::move(result.value());
        ASSERT_TRUE(pager->isOpen());

        // append an Table page (should be page 1)
        {
            auto result = pager->newPage(PageType::Table);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
            ASSERT_EQ(page.pageNum(), 1);
            ASSERT_EQ(page.pageType(), PageType::Table);
        }

        // append an Index page (should be page 2)
        {
            auto result = pager->newPage(PageType::Index);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
            ASSERT_EQ(page.pageNum(), 2);
            ASSERT_EQ(page.pageType(), PageType::Index);
        }

        // append 4 Table pages
        for (int i = 0; i < 4; ++i) {
            auto result = pager->newPage(PageType::Table);
            ASSERT_TRUE(result.isOk());
            auto page = std::move(result.value());
            ASSERT_EQ(page.pageNum(), 3 + i);
            ASSERT_EQ(page.pageType(), PageType::Table);
        }

        // close file
        ASSERT_TRUE(pager->close());

        // reopen file
        result = LRUPager::open(path, OpenMode::ReadOnly);
        ASSERT_TRUE(result.isOk());
        pager = std::move(result.value());
        ASSERT_TRUE(pager->isOpen());

        // check page 0
        {
            auto r = pager->fetchPage(0);
            ASSERT_TRUE(r.isOk());
            auto p = std::move(r.value());
            ASSERT_EQ(p.pageNum(), 0);
            ASSERT_EQ(p.pageType(), PageType::Table);
        }

        // check page 1
        {
            auto r = pager->fetchPage(1);
            ASSERT_TRUE(r.isOk());
            auto p = std::move(r.value());
            ASSERT_EQ(p.pageNum(), 1);
            ASSERT_EQ(p.pageType(), PageType::Table);
        }

        // check page 2
        {
            auto r = pager->fetchPage(2);
            ASSERT_TRUE(r.isOk());
            auto p = std::move(r.value());
            ASSERT_EQ(p.pageNum(), 2);
            ASSERT_EQ(p.pageType(), PageType::Index);
        }

        // check page 6
        {
            auto r = pager->fetchPage(6);
            ASSERT_TRUE(r.isOk());
            auto p = std::move(r.value());
            ASSERT_EQ(p.pageNum(), 6);
            ASSERT_EQ(p.pageType(), PageType::Table);
        }

        // close file
        ASSERT_TRUE(pager->close());
    }
    // pager goes out of scope and should close without error
}