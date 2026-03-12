#include "pager/lru_pager.h"
#include "utils/hexdump.h"

#include <filesystem>
#include <fstream>
#include <ctime>
#include <string>

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
    auto result = LRUPager::open(path, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // close Pager
    ASSERT_TRUE(pager->close());
}

TEST_F(LRUPagerTest, NewPagefetchPage) {
    auto result = LRUPager::open(path, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // append an Table page (should be page 1)
    auto r2 = pager->newPage(PageType::Table);
    ASSERT_TRUE(r2);
    auto page = std::move(r2.value());
    ASSERT_EQ(page.pageNum(), 1);
    ASSERT_EQ(page.pageType(), PageType::Table);

    // append an Index page (should be page 2)
    r2 = pager->newPage(PageType::Index);
    ASSERT_TRUE(r2);
    page = std::move(r2.value());
    ASSERT_EQ(page.pageNum(), 2);
    ASSERT_EQ(page.pageType(), PageType::Index);

    // append 4 Table pages
    for (int i = 0; i < 4; ++i) {
        auto result = pager->newPage(PageType::Table);
        ASSERT_TRUE(result.isOk());
        auto page = std::move(result.value());
        ASSERT_EQ(page.pageNum(), 3 + i);
        ASSERT_EQ(page.pageType(), PageType::Table);
    }

    // attempt to fetch a page out of range
    r2 = pager->fetchPage(99);
    ASSERT_FALSE(r2);
    ASSERT_EQ(r2.error().code(), Status::Code::InvalidArg);

    // close file
    page.reset();
    ASSERT_TRUE(pager->close());

    // reopen file
    {
        auto r1 = LRUPager::open(path, OpenMode::ReadOnly);
        ASSERT_TRUE(r1.isOk());
        auto readPager = std::move(r1.value());
        ASSERT_TRUE(readPager->isOpen());

        // check page 0
        r2 = readPager->fetchPage(0);
        ASSERT_TRUE(r2);
        auto readPage = std::move(r2.value());
        ASSERT_EQ(readPage.pageNum(), 0);
        ASSERT_EQ(readPage.pageType(), PageType::Table);

        // check page 1
        r2 = readPager->fetchPage(1);
        ASSERT_TRUE(r2);
        readPage = std::move(r2.value());
        ASSERT_EQ(readPage.pageNum(), 1);
        ASSERT_EQ(readPage.pageType(), PageType::Table);

        // check page 2
        r2 = readPager->fetchPage(2);
        ASSERT_TRUE(r2);
        readPage = std::move(r2.value());
        ASSERT_EQ(readPage.pageNum(), 2);
        ASSERT_EQ(readPage.pageType(), PageType::Index);

        // check page 6
        r2 = readPager->fetchPage(6);
        ASSERT_TRUE(r2);
        readPage = std::move(r2.value());
        ASSERT_EQ(readPage.pageNum(), 6);
        ASSERT_EQ(readPage.pageType(), PageType::Table);
    }
    // pager goes out of scope and should close without error
}

TEST_F(LRUPagerTest, DeletePage) {
    auto result = LRUPager::open(path, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // append 3 Table pages (pages 1-3)
    for (int i = 0; i < 3; ++i) {
        auto result = pager->newPage(PageType::Table);
        ASSERT_TRUE(result.isOk());
        auto page = std::move(result.value());
        ASSERT_EQ(page.pageNum(), 1 + i);
        ASSERT_EQ(page.pageType(), PageType::Table);
    }

    // delete page 2
    ASSERT_TRUE(pager->deletePage(2));

    // check type of page 2
    auto pageResult = pager->fetchPage(2);
    ASSERT_TRUE(pageResult);
    auto page = std::move(pageResult.value());
    ByteView view = page.subview(0, PAGE_HEADER_SIZE);
    PageType pageType = view.get<PageType>(offsetof(PageHeader, pageType));
    ASSERT_EQ(pageType, PageType::FreeList);

    // append another page (should reuse page 2)
    pageResult = pager->newPage(PageType::Index);
    ASSERT_TRUE(pageResult);
    page = std::move(pageResult.value());
    ASSERT_EQ(page.pageNum(), 2);
    ASSERT_EQ(page.pageType(), PageType::Index);

    // append another page (should be page 4)
    pageResult = pager->newPage(PageType::Table);
    ASSERT_TRUE(pageResult);
    page = std::move(pageResult.value());
    ASSERT_EQ(page.pageNum(), 4);
    ASSERT_EQ(page.pageType(), PageType::Table);

    // delete page 4 for manual inspection
    // should return InvalidArg error if PageGuard is still in scope
    auto errResult = pager->deletePage(4);
    ASSERT_FALSE(errResult);
    ASSERT_EQ(errResult.error().code(), Status::Code::InvalidArg);

    // reset PageGuard and then delete
    page.reset();
    ASSERT_TRUE(pager->deletePage(4));

    // close pager
    page.reset();
    ASSERT_TRUE(pager->close());

    // reopen pager to inspect FileHeader
    result = LRUPager::open(path, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // check freespaceHead in file header
    pageResult = pager->fetchPage(0);
    ASSERT_TRUE(pageResult);
    page = std::move(pageResult.value());
    view = page.subview(0, FILE_HEADER_SIZE);
    PageNum freespaceHead = view.get<PageNum>(offsetof(FileHeader, freespaceHead));
    ASSERT_EQ(freespaceHead, 4);

    // page goes out of scope and should unpin page
    // pager goes out of scope and should close without error
}

TEST_F(LRUPagerTest, PageEviction) {
    // open a Pager with a small pool size
    auto result = LRUPager::open(path, OpenMode::ReadWrite, 3);
    ASSERT_TRUE(result);
    auto pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());
    
    // byte patterns for testing
    uint64_t garbage = 0xEFCDAB8967452301;
    int sixseven = 0x6767;

    // write garbage to page 0
    {
        auto pageResult = pager->fetchPage(0);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 0);
        ASSERT_EQ(page.pageType(), PageType::Table);

        // get freeOffset
        ByteView view = page.subview(FILE_HEADER_SIZE, PAGE_HEADER_SIZE);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));

        // write byte pattern
        ByteSpan span = page.span();
        span.put<uint64_t>(offset, garbage);
        offset += sizeof(garbage);

        // set freeOffset
        span.put<PageOffset>(FILE_HEADER_SIZE + offsetof(PageHeader, freeOffset), offset);
    }

    // append one Index page
    {
        auto pageResult = pager->newPage(PageType::Index);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 1);
        ASSERT_EQ(page.pageType(), PageType::Index);
    }

    // append 9 Table pages with a known byte pattern for verification
    for (int i = 2; i < 11; ++i) {
        auto r = pager->newPage(PageType::Table);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());
        ASSERT_EQ(page.pageNum(), i);
        ASSERT_EQ(page.pageType(), PageType::Table);

        // get freeOffset
        ByteView view = page.subview(0, PAGE_HEADER_SIZE);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));

        // write byte pattern
        ByteSpan span = page.span();

        for (int j = 0; j < 10 + i; ++j) {
            span.put<int>(offset, sixseven);
            offset += sizeof(sixseven);
        }

        // set freeOffset
        span.put<PageOffset>(offsetof(PageHeader, freeOffset), offset);
    }

    // verify pages were flushed on eviction
    // check page 0 (Table Page)
    {
        auto pageResult = pager->fetchPage(0);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 0);
        ASSERT_EQ(page.pageType(), PageType::Table);

        // check garbage bytes
        ByteView view = page.subview(FILE_HEADER_SIZE, PAGE_ZERO_USABLE_SIZE);
        PageOffset offset = PAGE_HEADER_SIZE;
        uint64_t bytes = view.get<uint64_t>(offset);
        ASSERT_EQ(bytes, garbage);

        // check page 1 (Index page)
        pageResult = pager->fetchPage(1);
        ASSERT_TRUE(pageResult);
        page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 1);
        ASSERT_EQ(page.pageType(), PageType::Index);
    }

    // check pages 2-10 (Table Pages)
    for (int i = 2; i < 11; ++i) {
        auto r = pager->fetchPage(i);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());
        ASSERT_EQ(page.pageNum(), i);
        ASSERT_EQ(page.pageType(), PageType::Table);

        // verify byte pattern
        int sixseven = 0x6767;
        int data;
        ByteView view = page.span();
        PageOffset offset = PAGE_HEADER_SIZE;

        for (int j = 0; j < 10 + i; ++j) {
            data = view.get<int>(offset);
            ASSERT_EQ(data, sixseven);
            offset += sizeof(data);
        }

        // verify freeOffset
        PageOffset pageOffset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));
        ASSERT_EQ(pageOffset, offset);
    }

    ASSERT_TRUE(pager->flushAll());

    int64_t now = static_cast<int64_t>(std::time(nullptr));
    std::string hexdumpFileName = "../../../../debug/hexdump" + std::to_string(now) + ".log";
    std::ofstream hexdumpFile(hexdumpFileName);

    hexdumpFile << path << "\n";

    for (int i = 0; i < 11; ++i) {
        auto r = pager->fetchPage(i);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());

        hexdumpFile << "\n";
        hexdump(hexdumpFile, page.view(), i * PAGE_SIZE, "Page " + std::to_string(i));
    }
}