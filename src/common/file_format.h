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
inline constexpr uint16_t PAGE_HEADER_SIZE = 16;
inline constexpr uint16_t PAGE_FOOTER_SIZE = 4;
inline constexpr uint16_t PAGE_OVERHEAD = PAGE_HEADER_SIZE + PAGE_FOOTER_SIZE;

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
    uint8_t     magicBytes[8];
    uint16_t    fileFlags;
    uint8_t     versionMajor;
    uint8_t     versionMinor;
    uint32_t    pageCount;
    uint64_t    fileCreated;
    uint64_t    lastModified;
    PageNum     freespaceHead;
    uint32_t    checksum;
    uint8_t     reserved[24];   // pad to 64 bytes
};

static_assert(sizeof(FileHeader) == FILE_HEADER_SIZE);

struct PageHeader {
    PageType    type;
    uint8_t     flags;
    uint16_t    slotCount;
    uint16_t    freeOffset;
    uint8_t     level;
    uint8_t     reserved;
    PageNum     next;
    PageNum     prev;
};

static_assert(sizeof(PageHeader) == PAGE_HEADER_SIZE);

struct PageFooter {
    uint32_t    checksum;
};

static_assert(sizeof(PageFooter) == PAGE_FOOTER_SIZE);

} // namespace LiliumDB

#endif