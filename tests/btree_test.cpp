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
    auto to_bytes = [](std::string_view s) -> std::vector<uint8_t> {
        return {s.begin(), s.end()};
    };

    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> kvs = {
        {to_bytes("banana"),     to_bytes("yellow")},
        {to_bytes("elderberry"), to_bytes("black")},
        {to_bytes("date"),       to_bytes("brown")},
        {to_bytes("cherry"),     to_bytes("red")},
        {to_bytes("grape"),      to_bytes("green")},
        {to_bytes("apple"),      to_bytes("red")},
        {to_bytes("fig"),        to_bytes("purple")},
    };

    for (const auto& [key, value] : kvs) {
        ASSERT_TRUE(btree.insert(ByteView(key), ByteView(value)));
        printf("Inserted key: %s, value: %s\n", std::string(key.begin(), key.end()).c_str(), std::string(value.begin(), value.end()).c_str());
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
        {to_bytes("avocado"),       to_bytes("banana")},
        {to_bytes("blueberry"),     to_bytes("cherry")},
        {to_bytes("coconut"),       to_bytes("date")},
        {to_bytes("dragonfruit"),   to_bytes("elderberry")},
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