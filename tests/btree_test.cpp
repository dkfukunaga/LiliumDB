#include "test_helpers.h"
#include "btree/btree.h"
#include "pager/lru_pager.h"
#include "utils/hexdump.h"

#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"

using namespace LiliumDB;

class BTreeTest : public ::testing::Test {
protected:
    std::string testFileName = "test_db_file.db";
    std::unique_ptr<Pager> pager = nullptr;

    // helper function for converting strings to a vector of bytes
    static std::vector<uint8_t> toBytes(std::string_view s) {
        return std::vector<uint8_t>(s.begin(), s.end());
    }

    // helper function to convert std::vector<uint32_t> to std::vector<uint8_t>
    static std::vector<uint8_t> toLeBytes(std::vector<uint32_t> v) {
        std::vector<uint8_t> result;
        result.reserve(v.size() * sizeof(uint32_t));
        for (uint32_t val : v) {
            result.push_back(val & 0xFF);
            result.push_back((val >> 8) & 0xFF);
            result.push_back((val >> 16) & 0xFF);
            result.push_back((val >> 24) & 0xFF);
        }
        return result;
    }

    // helper function to pad keys to some amount of bytes bytes
    static std::vector<uint8_t> padKey(std::string_view key, size_t len, uint8_t pad = 0x00) {
        std::vector<uint8_t> result(len, pad);
        std::copy(key.begin(), key.end(), result.begin());
        return result;
    }

    void SetUp() override {
        if (std::filesystem::exists(testFileName)) {
            std::filesystem::remove(testFileName);
        }
    }

    void TearDown() override {
        
    }
};

TEST_F(BTreeTest, InsertSeek) {
    auto result = LRUPager::open(testFileName, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // create a BTree with root page 0
    BTree btree(*pager, 0, PageType::Table);

    // insert some key-value pairs
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> kvs = {
        {toBytes("banana"),     toBytes("yellow")},
        {toBytes("elderberry"), toBytes("black")},
        {toBytes("date"),       toBytes("brown")},
        {toBytes("cherry"),     toBytes("red")},
        {toBytes("grape"),      toBytes("green")},
        {toBytes("apple"),      toBytes("red")},
        {toBytes("fig"),        toBytes("purple")},
    };

    for (const auto& [key, value] : kvs) {
        ASSERT_TRUE(btree.insert(ByteView(key), ByteView(value)));
        // printf("Inserted key: %s, value: %s\n", std::string(key.begin(), key.end()).c_str(), std::string(value.begin(), value.end()).c_str());
    }

    // seek for existing keys
    for (const auto& [key, value] : kvs) {
        auto seekResult = btree.seek(ByteView(key));
        ASSERT_TRUE(seekResult.isOk());
        auto cursor = std::move(seekResult.value());
        ASSERT_TRUE(cursor.valid());
        ASSERT_EQ(cursor.key(), ByteView(key));
        ASSERT_EQ(cursor.value(), ByteView(value));
    }

    // seek for non-existing keys
    // should seek to the entry just after where it would fit
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> nonExistingKeys = {
        {toBytes("avocado"),       toBytes("banana")},
        {toBytes("blueberry"),     toBytes("cherry")},
        {toBytes("coconut"),       toBytes("date")},
        {toBytes("dragonfruit"),   toBytes("elderberry")},
    };

    for (const auto& [key1, key2] : nonExistingKeys) {
        auto seekResult = btree.seek(ByteView(key1));
        ASSERT_TRUE(seekResult.isOk());
        auto cursor = std::move(seekResult.value());
        ASSERT_TRUE(cursor.valid());
        ASSERT_EQ(cursor.key(), ByteView(key2));
    }

    // export hexdump
    testHexDump(*pager, testFileName);
}

TEST_F(BTreeTest, SplitAndInsert) {
    auto result = LRUPager::open(testFileName, OpenMode::ReadWrite);
    ASSERT_TRUE(result.isOk());
    pager = std::move(result.value());
    ASSERT_TRUE(pager->isOpen());

    // create a BTree with root page 0
    BTree btree(*pager, 0, PageType::Table);

    // create test entries
    size_t padding = 512;
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint32_t>>> kvs = {
        {padKey("01. just one", padding),               std::vector<uint32_t>(64, 0x00000001)},
        {padKey("02. six-seven", padding),              std::vector<uint32_t>(64, 0x00000067)},
        {padKey("03. two six-seven", padding),          std::vector<uint32_t>(64, 0x00670067)},
        {padKey("04. sixty-nine four-twenty", padding), std::vector<uint32_t>(64, 0x20046900)},
        {padKey("05. just letters", padding),           std::vector<uint32_t>(64, 0x00EFCDAB)},
        {padKey("06. lots of cafe babes", padding),     std::vector<uint32_t>(64, 0xBEBAFECA)},
        {padKey("07. lots of threes", padding),         std::vector<uint32_t>(64, 0x03030303)},
        {padKey("08. lucky number seven", padding),     std::vector<uint32_t>(64, 0x07777777)},
        {padKey("09. lucky number thirteen", padding),  std::vector<uint32_t>(64, 0x00130013)},
        {padKey("10. big entry 01", padding),           std::vector<uint32_t>(64, 0x00001111)},
        {padKey("11. big entry 02", padding),           std::vector<uint32_t>(64, 0x00011110)},
        {padKey("12. big entry 03", padding),           std::vector<uint32_t>(64, 0x00111100)},
        {padKey("13. big entry 04", padding),           std::vector<uint32_t>(64, 0x01111000)},
        {padKey("14. big entry 05", padding),           std::vector<uint32_t>(64, 0x11110000)},
        {padKey("15. big entry 06", padding),           std::vector<uint32_t>(64, 0x00002222)},
        {padKey("16. big entry 07", padding),           std::vector<uint32_t>(64, 0x00022220)},
        {padKey("17. big entry 08", padding),           std::vector<uint32_t>(64, 0x00222200)},
        {padKey("18. big entry 09", padding),           std::vector<uint32_t>(64, 0x02222000)},
        {padKey("19. big entry 10", padding),           std::vector<uint32_t>(64, 0x22220000)},
        {padKey("20. big entry 11", padding),           std::vector<uint32_t>(64, 0x00003333)},
        {padKey("21. big entry 12", padding),           std::vector<uint32_t>(64, 0x00033330)},
        {padKey("22. big entry 13", padding),           std::vector<uint32_t>(64, 0x00333300)},
        {padKey("23. big entry 14", padding),           std::vector<uint32_t>(64, 0x03333000)},
        {padKey("24. big entry 15", padding),           std::vector<uint32_t>(64, 0x33330000)},
        {padKey("25. big entry 16", padding),           std::vector<uint32_t>(64, 0x00004444)},
        {padKey("26. big entry 17", padding),           std::vector<uint32_t>(64, 0x00044440)},
        {padKey("27. big entry 18", padding),           std::vector<uint32_t>(64, 0x00444400)},
        {padKey("28. big entry 19", padding),           std::vector<uint32_t>(64, 0x04444000)},
        {padKey("29. big entry 20", padding),           std::vector<uint32_t>(64, 0x44440000)},
    };

    for (const auto& [key, value] : kvs) {
        ASSERT_TRUE(btree.insert(ByteView(key), ByteView(toLeBytes(value))));
    }

    // should split once, plush pages and check page count
    ASSERT_TRUE(pager->flushAll());
    ASSERT_EQ(pager->pageCount(), 12);
    
    // export hexdump
    testHexDump(*pager, testFileName);
}