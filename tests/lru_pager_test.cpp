#include "pager/lru_pager.h"
#include "utils/hexdump.h"
#include "utils/byte_span_macros.h"

#include <filesystem>
#include <fstream>
#include <ctime>
#include <string>
#include <cmath>

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
    auto pageResult = pager->newPage(PageType::Table);
    ASSERT_TRUE(pageResult);
    auto page = std::move(pageResult.value());
    ASSERT_EQ(page.pageNum(), 1);
    // check page type
    auto header = page.view().get<PageHeader>(0);
    ASSERT_EQ(header.pageType, PageType::Table);

    // append an Index page (should be page 2)
    pageResult = pager->newPage(PageType::Index);
    ASSERT_TRUE(pageResult);
    page = std::move(pageResult.value());
    // check page type
    header = page.view().get<PageHeader>(0);
    ASSERT_EQ(header.pageType, PageType::Index);

    // append 4 Table pages
    for (int i = 0; i < 4; ++i) {
        auto result = pager->newPage(PageType::Table);
        ASSERT_TRUE(result.isOk());
        auto page = std::move(result.value());
        ASSERT_EQ(page.pageNum(), 3 + i);
    }

    // attempt to fetch a page out of range
    pageResult = pager->fetchPage(99);
    ASSERT_FALSE(pageResult);
    ASSERT_EQ(pageResult.error().code(), Status::Code::InvalidArg);

    // verify page header and footer

    // page 0
    (void)pager->flushAll();
    pageResult = pager->fetchPage(0);
    ASSERT_TRUE(pageResult);
    page = std::move(pageResult.value());

    // verify header
    ByteView headerView = page.subview(PAGE_ZERO_OFFSET, sizeof(PageHeader));
    header = headerView.get<PageHeader>(0);

    ASSERT_EQ(header.pageType, PageType::Table);
    ASSERT_EQ(header.pageFlags, PageFlags());
    ASSERT_EQ(header.level, INVALID_TREE_LEVEL);
    ASSERT_EQ(header.slotCount, 0);
    ASSERT_EQ(header.freeOffset, PAGE_ZERO_OFFSET + PAGE_HEADER_SIZE);
    ASSERT_EQ(header.next, INVALID_PAGE);
    ASSERT_EQ(header.prev, INVALID_PAGE);
    
    // verify footer
    PageFooter footer;
    ByteView footerView = page.subview(PAGE_END_OFFSET, sizeof(PageFooter));
    footerView.get<PageFooter>(0, footer);
    ASSERT_EQ(footer.checksum, CHECKSUM_PLACEHOLDER);

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
        pageResult = readPager->fetchPage(0);
        ASSERT_TRUE(pageResult);
        auto readPage = std::move(pageResult.value());
        ASSERT_EQ(readPage.pageNum(), 0);
        // check page type
        auto header = readPage.view().get<PageHeader>(PAGE_ZERO_OFFSET);
        ASSERT_EQ(header.pageType, PageType::Table);

        // check page 1
        pageResult = readPager->fetchPage(1);
        ASSERT_TRUE(pageResult);
        readPage = std::move(pageResult.value());
        ASSERT_EQ(readPage.pageNum(), 1);
        // check page type
        header = readPage.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Table);

        // check page 2
        pageResult = readPager->fetchPage(2);
        ASSERT_TRUE(pageResult);
        readPage = std::move(pageResult.value());
        ASSERT_EQ(readPage.pageNum(), 2);
        header = readPage.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Index);

        // check page 6
        pageResult = readPager->fetchPage(6);
        ASSERT_TRUE(pageResult);
        readPage = std::move(pageResult.value());
        ASSERT_EQ(readPage.pageNum(), 6);
        header = readPage.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Table);
    }
    // pager goes out of scope and should close without error
}

TEST_F(LRUPagerTest, DeletePage) {
    auto result = LRUPager::open(path, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    int sixseven = 0x6767;

    // append 4 Table pages (pages 1-4) with a byte pattern
    for (int i = 1; i < 5; ++i) {
        auto result = pager->newPage(PageType::Table);
        ASSERT_TRUE(result.isOk());
        auto page = std::move(result.value());
        ASSERT_EQ(page.pageNum(), i);
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Table);

        // verify header
        ByteView view = page.view();
        auto pageType = view.get<PageType>(offsetof(PageHeader, pageType));
        ASSERT_EQ(pageType, PageType::Table);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));
        ASSERT_EQ(offset, PAGE_HEADER_SIZE);

        // write byte pattern
        ByteSpan span = page.span();

        for (int j = 0; j < i*i; ++j) {
            span.put<int>(offset, sixseven);
            offset += sizeof(sixseven);
            if (offset >= PAGE_END_OFFSET) break;
        }

        // set freespace offset
        span.put<PageOffset>(offsetof(PageHeader, freeOffset), offset);
    }

    // delete page 2
    ASSERT_TRUE(pager->deletePage(2));

    // check type of page 2
    {
        auto pageResult = pager->fetchPage(2);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::FreeList);
    }

    // append another page (should reuse page 2)
    {
        auto pageResult = pager->newPage(PageType::Index);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 2);
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Index);

        // confirm page header reset
        ByteView view = page.view();
        auto pageType = view.get<PageType>(offsetof(PageHeader, pageType));
        ASSERT_EQ(pageType, PageType::Index);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));
        ASSERT_EQ(offset, PAGE_HEADER_SIZE);

        // confirm useable space has been zeroed
        uint32_t data;
        for (int i = offset; i < view.size() - PAGE_FOOTER_SIZE; i += sizeof(data)) {
            data = view.get<uint32_t>(i);
                ASSERT_EQ(data, 0);
        }
    }

    // append another page (should be page 5)
    auto pageResult = pager->newPage(PageType::Table);
    ASSERT_TRUE(pageResult);
    auto page = std::move(pageResult.value());
    ASSERT_EQ(page.pageNum(), 5);
    auto header = page.view().get<PageHeader>(0);
    ASSERT_EQ(header.pageType, PageType::Table);

    // delete page 4 for manual inspection
    // should return InvalidArg error if PageGuard is still in scope
    auto errResult = pager->deletePage(5);
    ASSERT_FALSE(errResult);
    ASSERT_EQ(errResult.error().code(), Status::Code::InvalidArg);

    // reset PageGuard and then delete
    page.reset();
    ASSERT_TRUE(pager->deletePage(5));

    // delete page 4
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
    {
        auto pageResult = pager->fetchPage(0);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        auto view = page.subview(0, FILE_HEADER_SIZE);
        PageNum freespaceHead = view.get<PageNum>(offsetof(FileHeader, freespaceHead));
        ASSERT_EQ(freespaceHead, 4);
    }

    // check next field on page 4, should point to page 5
    {
        auto pageResult = pager->fetchPage(4);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        auto view = page.subview(0, PAGE_HEADER_SIZE);

        auto type = view.get<PageType>(offsetof(PageHeader, pageType));
        ASSERT_EQ(type, PageType::FreeList);
        auto nextFree = view.get<PageNum>(offsetof(PageHeader, next));
        ASSERT_EQ(nextFree, 5);
    }
    
    // hexdump to a text file to manually inspect
    auto now = std::time(nullptr);
    std::string hexdumpFileName = "../../../../debug/hexdump_" + std::to_string(now) + ".hex";
    std::ofstream hexdumpFile(hexdumpFileName);

    hexdumpFile << path << "\n\n";

    for (int i = 0; i < pager->pageCount(); ++i) {
        auto r = pager->fetchPage(i);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());

        hexdump(hexdumpFile, page.view(), i * PAGE_SIZE, "Page " + std::to_string(i));
    }

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
    const char* song =
        "This is the song that doesn't end\n"
        "Yes, it goes on and on, my friend\n"
        "Some people started singing it not knowing what it was,\n"
        "And they'll continue singing it forever just because";
    uint16_t songLen = strlen(song);

    // write garbage to page 0
    {
        auto pageResult = pager->fetchPage(0);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 0);
        auto header = page.view().get<PageHeader>(PAGE_ZERO_OFFSET);
        ASSERT_EQ(header.pageType, PageType::Table);

        // get freeOffset
        ByteView view = page.subview(FILE_HEADER_SIZE, PAGE_HEADER_SIZE);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));

        // write byte pattern
        ByteSpan span = page.span();
        span.put<uint64_t>(offset, garbage);
        offset += sizeof(garbage);
        span.put<uint16_t>(offset, songLen);
        offset += sizeof(songLen);
        span.write(offset, reinterpret_cast<const uint8_t*>(song), songLen);
        offset += songLen;

        // set freeOffset
        span.put<PageOffset>(FILE_HEADER_SIZE + offsetof(PageHeader, freeOffset), offset);
    }

    // append one Index page
    {
        auto pageResult = pager->newPage(PageType::Index);
        ASSERT_TRUE(pageResult);
        auto page = std::move(pageResult.value());
        ASSERT_EQ(page.pageNum(), 1);
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Index);
    }

    // append 9 Table pages with a known byte pattern for verification
    for (int i = 2; i < 11; ++i) {
        auto r = pager->newPage(PageType::Table);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());
        ASSERT_EQ(page.pageNum(), i);
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Table);

        // get freeOffset
        ByteView view = page.subview(0, PAGE_HEADER_SIZE);
        auto offset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));

        // write byte pattern
        ByteSpan span = page.span();

        for (int j = 0; j < static_cast<int>(std::pow(2, i)); ++j) {
            span.put<int>(offset, sixseven);
            offset += sizeof(sixseven);
            if (offset >= PAGE_END_OFFSET) break;
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
        auto header = page.view().get<PageHeader>(PAGE_ZERO_OFFSET);
        ASSERT_EQ(header.pageType, PageType::Table);

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
        header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Index);
    }

    // check pages 2-10 (Table Pages)
    for (int i = 2; i < 11; ++i) {
        auto r = pager->fetchPage(i);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());
        ASSERT_EQ(page.pageNum(), i);
        auto header = page.view().get<PageHeader>(0);
        ASSERT_EQ(header.pageType, PageType::Table);

        // verify byte pattern
        int data;
        ByteView view = page.span();
        PageOffset offset = PAGE_HEADER_SIZE;

        for (int j = 0; j < static_cast<int>(std::pow(2, i)); ++j) {
            data = view.get<int>(offset);
            ASSERT_EQ(data, sixseven);
            offset += sizeof(data);
            if (offset >= PAGE_END_OFFSET) break;
        }

        // verify freeOffset
        PageOffset pageOffset = view.get<PageOffset>(offsetof(PageHeader, freeOffset));
        ASSERT_EQ(pageOffset, offset);
    }

    ASSERT_TRUE(pager->flushAll());

    auto now = std::time(nullptr);
    std::string hexdumpFileName = "../../../../debug/hexdump_" + std::to_string(now) + ".hex";
    std::ofstream hexdumpFile(hexdumpFileName);

    hexdumpFile << path << "\n\n";

    for (int i = 0; i < pager->pageCount(); ++i) {
        auto r = pager->fetchPage(i);
        ASSERT_TRUE(r);
        auto page = std::move(r.value());

        hexdump(hexdumpFile, page.view(), i * PAGE_SIZE, "Page " + std::to_string(i));
    }
}