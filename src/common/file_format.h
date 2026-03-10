#ifndef LILIUMDB_FILE_FORMAT_H
#define LILIUMDB_FILE_FORMAT_H

#include <cstdint>
#include <array>

#include "types.h"
#include "utils/flags.h"

namespace LiliumDB {

//  magic bytes: CAT_CAFE LDB\0
inline constexpr std::array<uint8_t, 8> MAGIC_BYTES = {0xCA, 0x70, 0xCA, 0xFE, 0x4C, 0x44, 0x42, 0x00};

// current version is 0.0
inline constexpr uint8_t VERSION_MAJOR = 0;
inline constexpr uint8_t VERSION_MINOR = 0;

// checksum placeholder value
inline constexpr uint32_t CHECKSUM_PLACEHOLDER = 0xEFBEADDE;

// page size
inline constexpr uint16_t PAGE_SIZE = 4096;

// header sizes
inline constexpr uint16_t FILE_HEADER_SIZE = 64;
inline constexpr uint16_t PAGE_HEADER_SIZE = 20;

// page usable size
inline constexpr uint16_t PAGE_USABLE_SIZE = PAGE_SIZE - PAGE_HEADER_SIZE;
inline constexpr uint16_t PAGE_ZERO_USABLE_SIZE = PAGE_SIZE - FILE_HEADER_SIZE - PAGE_HEADER_SIZE;

enum class FileFlag : uint16_t {
    None     = 0x00,
    ReadOnly = 0x01,
    Corrupt  = 0x02,
};

using FileFlags = Flags<FileFlag>;

struct FileHeader {
    uint8_t     magicBytes[8];
    FileFlags   fileFlags;
    uint8_t     versionMajor;
    uint8_t     versionMinor;
    uint32_t    pageCount;
    int64_t     fileCreated;    // seconds since unix epoch
    int64_t     lastModified;   // seconds since unix epoch
    PageNum     freespaceHead;
    uint8_t     reserved[24];   // pad to 64 bytes
    uint32_t    checksum;       // always last; will implement later
};

static_assert(sizeof(FileHeader) == FILE_HEADER_SIZE);

enum class PageType : uint8_t {
    Invalid     = 0,    // invalid/unitialized
    Table       = 1,    // table page
    Index       = 2,    // index page
    Expansion   = 3,    // overflow (future)
    FreeList    = 4,    // free list
    FreeSpace   = 5,    // Freespace map (future)
};

enum class PageFlag : uint8_t {
    None     = 0x00,
    Corrupt  = 0x01,
};

inline constexpr uint8_t INVALID_PAGE_LEVEL = std::numeric_limits<uint8_t>::max();

using PageFlags = Flags<PageFlag>;

struct PageHeader {
    PageType    pageType;
    PageFlags   pageFlags;
    uint8_t     level;          // for B+ tree - 0 indicates leaf page
    uint8_t     reserved;       // reserved for fragmentCount in RecordStore (future)
    uint16_t    slotCount;      // used by RecordStore
    uint16_t    freeOffset;     // used by RecordStore
    PageNum     next;           // right child for B+ tree branch pages
    PageNum     prev;
    uint32_t    checksum;       // reserved; will implement later
};

static_assert(sizeof(PageHeader) == PAGE_HEADER_SIZE);

} // namespace LiliumDB

#endif