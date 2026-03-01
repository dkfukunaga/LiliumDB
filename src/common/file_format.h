#ifndef LILIUMDB_FILE_FORMAT_H
#define LILIUMDB_FILE_FORMAT_H

#include <cstdint>
#include <array>
#include <limits>

#include "types.h"

namespace LiliumDB {

//  magic bytes: CAT_CAFE LDB\0
inline constexpr std::array<uint8_t, 8> MAGIC = {0xCA, 0x70, 0xCA, 0xFE, 0x4C, 0x44, 0x42, 0x00};

// page size
inline constexpr uint16_t PAGE_SIZE = 4096;

// header sizes
inline constexpr uint16_t FILE_HEADER_SIZE = 64;
inline constexpr uint16_t PAGE_HEADER_SIZE = 8;
inline constexpr uint16_t BRANCH_HEADER_SIZE = 4;
inline constexpr uint16_t LEAF_HEADER_SIZE = 8;
inline constexpr uint16_t PAGE_FOOTER_SIZE = 4;
inline constexpr uint16_t PAGE_OVERHEAD = PAGE_HEADER_SIZE + PAGE_FOOTER_SIZE;
inline constexpr uint16_t BRANCH_OVERHEAD = PAGE_OVERHEAD + BRANCH_HEADER_SIZE;
inline constexpr uint16_t LEAF_OVERHEAD = PAGE_OVERHEAD + LEAF_HEADER_SIZE;

// page 0 offset to usable data region
inline constexpr PageOffset PAGE_ZERO_OFFSET = FILE_HEADER_SIZE;

// sentinel values for invalid offsets
inline constexpr PageNum INVALID_PAGE = std::numeric_limits<PageNum>::max();
inline constexpr SlotNum INVALID_SLOT = std::numeric_limits<SlotNum>::max();

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
    uint32_t    page_count;
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
    PageOffset  free_offset;
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