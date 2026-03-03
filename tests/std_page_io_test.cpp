#include "storage/page_io.h"
#include "storage/std_page_io.h"
#include "common/file_format.h"

#include <vector>
#include <memory>
#include <filesystem>

#include <gtest/gtest.h>

using namespace LiliumDB;
using Code = Status::Code;

class StdPageIOTest : public ::testing::Test {
protected:
    std::string path = "test_page_io.dat";
    std::unique_ptr<PageIO> pageIO = nullptr;

    void SetUp() override {
        pageIO = std::make_unique<StdPageIO>();
    }

    void TearDown() override {
        std::filesystem::remove(path);
    }
};

TEST_F(StdPageIOTest, OpenClose) {
    EXPECT_FALSE(pageIO->isOpen());

    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Close the file
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
    EXPECT_FALSE(pageIO->isOpen());
}

TEST_F(StdPageIOTest, ReadWritePage) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page
    PageNum pageNum = 0;
    EXPECT_EQ(pageIO->writePage(pageNum, src).code(), Code::Ok);

    // Prepare a buffer to read into
    std::vector<uint8_t> readData(PAGE_SIZE);
    ByteSpan dst(readData.data(), readData.size());

    // Read the page back
    EXPECT_EQ(pageIO->readPage(pageNum, dst).code(), Code::Ok);

    // Verify the data matches what was written
    EXPECT_TRUE(std::equal(src.begin(), src.end(), dst.begin()));

    // Clean up
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, WriteReadOnly) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page
    PageNum pageNum = 0;
    EXPECT_EQ(pageIO->writePage(pageNum, src).code(), Code::Ok);

    // Close the file
    EXPECT_EQ(pageIO->close().code(), Code::Ok);

    // Reopen the file in read-only mode
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadOnly).code(), Code::Ok);
    EXPECT_TRUE(pageIO->isOpen());

    // Attempt to write to the read-only file
    EXPECT_EQ(pageIO->writePage(pageNum, src).code(), Code::FileErr);

    // Clean up
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, OpenNonExistentReadOnly) {
    // Attempt to open a non-existent file in read-only mode
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadOnly).code(), Code::FileErr);
    EXPECT_FALSE(pageIO->isOpen());
}

TEST_F(StdPageIOTest, OffsetCorrectness) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare two pages of data to write
    std::vector<uint8_t> page1(PAGE_SIZE, 0x11);
    std::vector<uint8_t> page2(PAGE_SIZE, 0x22);
    ByteView src1(page1.data(), page1.size());
    ByteView src2(page2.data(), page2.size());

    // Write the first page
    EXPECT_EQ(pageIO->writePage(0, src1).code(), Code::Ok);

    // Write the second page
    EXPECT_EQ(pageIO->writePage(1, src2).code(), Code::Ok);

    // Prepare buffers to read into
    std::vector<uint8_t> readData1(PAGE_SIZE);
    std::vector<uint8_t> readData2(PAGE_SIZE);
    ByteSpan dst1(readData1.data(), readData1.size());
    ByteSpan dst2(readData2.data(), readData2.size());

    // Read the first page back
    EXPECT_EQ(pageIO->readPage(0, dst1).code(), Code::Ok);
    EXPECT_TRUE(std::equal(src1.begin(), src1.end(), dst1.begin()));

    // Read the second page back
    EXPECT_EQ(pageIO->readPage(1, dst2).code(), Code::Ok);
    EXPECT_TRUE(std::equal(src2.begin(), src2.end(), dst2.begin()));

    // Clean up
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, OutOfBounds) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a buffer to read into
    std::vector<uint8_t> readData(PAGE_SIZE);
    ByteSpan dst(readData.data(), readData.size());

    // Attempt to read from an out-of-bounds page
    EXPECT_EQ(pageIO->readPage(1, dst).code(), Code::IOErr);
    EXPECT_EQ(pageIO->readPage(5, dst).code(), Code::IOErr);

    // Clean up
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, FileSizeVerification) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Prepare a page of data to write
    std::vector<uint8_t> writeData(PAGE_SIZE, 0x67);
    ByteView src(writeData.data(), writeData.size());

    // Write the page 5 times
    for (PageNum pageNum = 0; pageNum < 5; ++pageNum) {
        EXPECT_EQ(pageIO->writePage(pageNum, src).code(), Code::Ok);
    }

    // Verify the file size is correct (5 pages)
    std::filesystem::path filePath(path);
    EXPECT_EQ(std::filesystem::file_size(filePath), 5 * PAGE_SIZE);

    // Close the file
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, CloseWithoutOpen) {
    // Attempt to close without opening a file
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}

TEST_F(StdPageIOTest, ReadWithoutOpen) {
    // Prepare a buffer to read into
    std::vector<uint8_t> readData(PAGE_SIZE);
    ByteSpan dst(readData.data(), readData.size());

    // Attempt to read without opening a file
    EXPECT_EQ(pageIO->readPage(0, dst).code(), Code::FileErr);
}

TEST_F(StdPageIOTest, OpenAlreadyOpen) {
    // Open a new file for writing
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::NewFile);
    EXPECT_TRUE(pageIO->isOpen());

    // Attempt to open the file again
    EXPECT_EQ(pageIO->open(path, OpenMode::ReadWrite).code(), Code::FileErr);
    EXPECT_TRUE(pageIO->isOpen());

    // Clean up
    EXPECT_EQ(pageIO->close().code(), Code::Ok);
}