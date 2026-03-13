#ifndef LILIUMDB_FILE_FORMAT_H
#define LILIUMDB_FILE_FORMAT_H

#include <cstdint>

#include "types.h"
#include "utils/flags.h"

namespace LiliumDB {

//  magic bytes: CAT_CAFE LDB\0
inline constexpr uint8_t MAGIC_BYTES[] = {0xCA, 0x70, 0xCA, 0xFE, 0x4C, 0x44, 0x42, 0x00};
inline constexpr size_t MAGIC_BYTES_LEN = sizeof(MAGIC_BYTES) / sizeof(MAGIC_BYTES[0]);

// checksum placeholder value
inline constexpr uint32_t CHECKSUM_PLACEHOLDER = 0xA71C555A;

// current version is 0.0
inline constexpr uint8_t VERSION_MAJOR = 0;
inline constexpr uint8_t VERSION_MINOR = 0;

// page size
inline constexpr uint16_t PAGE_SIZE = 4096;

// header sizes
inline constexpr uint16_t FILE_HEADER_SIZE = 64;
inline constexpr uint16_t PAGE_HEADER_SIZE = 16;
inline constexpr uint16_t PAGE_FOOTER_SIZE = 4;

// page usable size
inline constexpr uint16_t PAGE_USABLE_SIZE = PAGE_SIZE - PAGE_HEADER_SIZE;
inline constexpr uint16_t PAGE_ZERO_USABLE_SIZE = PAGE_SIZE - FILE_HEADER_SIZE - PAGE_HEADER_SIZE;

// page offsets
inline constexpr PageOffset PAGE_ZERO_OFFSET = FILE_HEADER_SIZE;
inline constexpr PageOffset PAGE_END_OFFSET = PAGE_SIZE - PAGE_FOOTER_SIZE;

enum class FileFlag : uint16_t {
    None     = 0x00,
    ReadOnly = 0x01,
    Corrupt  = 0x02,
};

using FileFlags = Flags<FileFlag>;

struct FileHeader {
    uint8_t     magicBytes[8] = {0xCA, 0x70, 0xCA, 0xFE, 0x4C, 0x44, 0x42, 0x00};
    FileFlags   fileFlags = FileFlags();
    uint8_t     versionMajor = VERSION_MAJOR;
    uint8_t     versionMinor = VERSION_MINOR;
    uint32_t    pageCount = 0;
    int64_t     fileCreated = 0;                     // seconds since unix epoch
    int64_t     lastModified = 0;                   // seconds since unix epoch
    PageNum     freespaceHead = INVALID_PAGE;
    uint8_t     reserved[24] = {0};                 // pad to 64 bytes
    uint32_t    checksum = CHECKSUM_PLACEHOLDER;    // always last; will implement later
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
inline constexpr PageOffset INVALID_PAGE_OFFSET = std::numeric_limits<PageOffset>::max();

using PageFlags = Flags<PageFlag>;

struct PageHeader {
    PageType    pageType = PageType::Invalid;
    PageFlags   pageFlags = PageFlags();
    uint8_t     level = INVALID_PAGE_LEVEL;         // for B+ tree - 0 indicates leaf page
    uint8_t     reserved = 0;                       // reserved for fragmentCount in RecordStore (future)
    uint16_t    slotCount = 0;                      // used by RecordStore
    PageOffset  freeOffset = INVALID_PAGE_OFFSET;   // used by RecordStore
    PageNum     next = INVALID_PAGE;                // right child for B+ tree branch pages
    PageNum     prev = INVALID_PAGE;
};

static_assert(sizeof(PageHeader) == PAGE_HEADER_SIZE);

struct PageFooter {
    uint32_t    checksum = CHECKSUM_PLACEHOLDER;    // reserved; will implement later
};

static_assert(sizeof(PageFooter) == PAGE_FOOTER_SIZE);

} // namespace LiliumDB

#endif