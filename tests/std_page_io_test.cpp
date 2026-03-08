#include "storage/page_io.h"
#include "storage/std_page_io.h"
#include "common/file_format.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using namespace LiliumDB;
using Code = Status::Code;

class StdPageIOTest : public ::testing::Test {
protected:
    std::string path = "test_page_io.dat";
    std::unique_ptr<PageIO> pageIO = nullptr;

    // void SetUp() override {
    //     pageIO = std::make_unique<StdPageIO>();
    // }

    void TearDown() override {
        std::filesystem::remove(path);
    }
};

TEST_F(StdPageIOTest, OpenClose) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Close the file
    EXPECT_TRUE(pageIO->close());
    EXPECT_FALSE(pageIO->isOpen());
}

TEST_F(StdPageIOTest, ReadWritePage) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page
    PageNum pageNum = 0;
    EXPECT_TRUE(pageIO->writePage(pageNum, src));

    // Prepare a buffer to read into
    std::vector<uint8_t> readData(PAGE_SIZE);
    ByteSpan dst(readData.data(), readData.size());

    // Read the page back
    EXPECT_TRUE(pageIO->readPage(pageNum, dst).isOk());

    // Verify the data matches what was written
    EXPECT_TRUE(std::equal(src.begin(), src.end(), dst.begin()));

    // Clean up
    EXPECT_TRUE(pageIO->close());
}

TEST_F(StdPageIOTest, WriteReadOnly) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page
    PageNum pageNum = 0;
    EXPECT_TRUE(pageIO->writePage(pageNum, src));

    // Close the file
    EXPECT_TRUE(pageIO->close());

    // Construct a new PageIO reoping the file in read-only mode
    r = StdPageIO::open(path, OpenMode::ReadOnly);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Attempt to write to the read-only file
    EXPECT_EQ(pageIO->writePage(pageNum, src).error().code(), Code::FileErr);

    // Clean up
    EXPECT_TRUE(pageIO->close());
}

TEST_F(StdPageIOTest, OpenNonExistentReadOnly) {
    // Attempt to open a non-existent file in read-only mode
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadOnly);
    EXPECT_FALSE(r.isOk());
    EXPECT_TRUE(r.isErr());
}

TEST_F(StdPageIOTest, OpenFailsOnIncorrectFileSize) {
    // Prepare incorrectly sized page to write
    uint16_t wrongSize = PAGE_SIZE + 117;
    std::vector<uint8_t> page(wrongSize, 0x67);

    // Write to file directly with fstream
    std::fstream file(path, std::ios::binary | std::ios::out);
    file.write(reinterpret_cast<char *>(page.data()), page.size());
    file.close();

    // Construct a new PageIO for writing to file
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.error().code(), Status::Code::FileErr);
}

TEST_F(StdPageIOTest, OffsetCorrectness) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare two pages of data to write
    std::vector<uint8_t> page1(PAGE_SIZE, 0x11);
    std::vector<uint8_t> page2(PAGE_SIZE, 0x22);
    ByteView src1(page1.data(), page1.size());
    ByteView src2(page2.data(), page2.size());

    // Write the first page
    EXPECT_TRUE(pageIO->writePage(0, src1));

    // Write the second page
    EXPECT_TRUE(pageIO->writePage(1, src2));

    // Prepare buffers to read into
    std::vector<uint8_t> readData1(PAGE_SIZE);
    std::vector<uint8_t> readData2(PAGE_SIZE);
    ByteSpan dst1(readData1.data(), readData1.size());
    ByteSpan dst2(readData2.data(), readData2.size());

    // Read the first page back
    EXPECT_TRUE(pageIO->readPage(0, dst1));
    EXPECT_TRUE(std::equal(src1.begin(), src1.end(), dst1.begin()));

    // Read the second page back
    EXPECT_TRUE(pageIO->readPage(1, dst2));
    EXPECT_TRUE(std::equal(src2.begin(), src2.end(), dst2.begin()));

    // Clean up
    EXPECT_TRUE(pageIO->close());
}

TEST_F(StdPageIOTest, OutOfBounds) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a buffer to read into
    std::vector<uint8_t> readData(PAGE_SIZE);
    ByteSpan dst(readData.data(), readData.size());

    // Attempt to read from an out-of-bounds page
    EXPECT_EQ(pageIO->readPage(1, dst).error().code(), Code::IOErr);
    EXPECT_EQ(pageIO->readPage(5, dst).error().code(), Code::IOErr);

    // Clean up
    EXPECT_TRUE(pageIO->close());
}

TEST_F(StdPageIOTest, FileSizeVerification) {
    // Construct a new PageIO for writing
    auto r = StdPageIO::open(path, OpenMode::ReadWrite);
    EXPECT_TRUE(r.isOk());
    pageIO = std::move(r.value());
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page 5 times
    for (PageNum pageNum = 0; pageNum < 5; ++pageNum) {
        EXPECT_TRUE(pageIO->writePage(pageNum, src));
    }

    // Verify the file size is correct (5 pages)
    std::filesystem::path filePath(path);
    EXPECT_EQ(std::filesystem::file_size(filePath), 5 * PAGE_SIZE);

    // Close the file
    EXPECT_TRUE(pageIO->close());
}