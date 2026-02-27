#ifndef LILIUMDB_FILE_FORMAT_H
#define LILIUMDB_FILE_FORMAT_H

#include <array>

#include "common.h"

namespace LiliumDB {

//  CAT_CAFE LDB\0
static constexpr std::array<uint8_t, 8> MAGIC = {0xCA, 0x70, 0xCA, 0xFE, 0x4C, 0x44, 0x42, 0x00};
static constexpr size_t FILE_HEADER_SIZE = 64;
static constexpr size_t PAGE_HEADER_SIZE = 8;
static constexpr size_t BRANCH_HEADER_SIZE = 4;
static constexpr size_t LEAF_HEADER_SIZE = 8;
static constexpr size_t PAGE_FOOTER_SIZE = 4;
static constexpr size_t PAGE_OVERHEAD = PAGE_HEADER_SIZE + PAGE_FOOTER_SIZE;
static constexpr size_t BRANCH_OVERHEAD = PAGE_OVERHEAD + BRANCH_HEADER_SIZE;
static constexpr size_t LEAF_OVERHEAD = PAGE_OVERHEAD + LEAF_HEADER_SIZE;
static constexpr size_t PAGE_ZERO_OFFSET = PAGE_HEADER_SIZE;

enum class PageType : uint8_t {
    Invalid = 0,    // invalid/unitialized
    Table,          // Table B+ Tree
    Index,          // Index B+ Tree
    Expansion,      // overflow (future)
    FreeSpace,      // Freespace map (future)
};

struct FileHeader {
    uint8_t     magic_bytes[8];
    uint16_t    file_flags;
    uint8_t     version_major;
    uint8_t     version_minor;
    uint32_t    num_pages;
    uint64_t    file_created;
    uint64_t    last_modified;
    PageNum     freespace_head;
    uint32_t    checksum;
    uint8_t     reserved[24];   // pad to 64 bytes
};

static_assert(sizeof(FileHeader) == FILE_HEADER_SIZE);

struct PageHeader {
    PageType    page_type;
    uint8_t     page_flags;
    uint8_t     level;          // used by BPTree, 0 = leaf
    uint8_t     reserved;
    uint16_t    slot_count;
    uint16_t    free_offset;
};

static_assert(sizeof(PageHeader) == PAGE_HEADER_SIZE);

struct BranchHeader {
    PageNum     right_child;
};

static_assert(sizeof(BranchHeader) == BRANCH_HEADER_SIZE);

struct LeafHeader {
    PageNum     next_page;
    PageNum     prev_page;
};

static_assert(sizeof(LeafHeader) == LEAF_HEADER_SIZE);

struct PageFooter {
    uint32_t    checksum;
};

static_assert(sizeof(PageFooter) == PAGE_FOOTER_SIZE);

} // namespace LiliumDB

#endif