#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>
#include <limits>

namespace LiliumDB {

using PageNum = uint32_t;
using SlotNum = uint16_t;
using PageOffset = uint16_t;
using RecordSize = uint16_t;

enum class PageType : uint8_t {
    Invalid     = 0,    // invalid/unitialized
    Table       = 1,    // table page
    Index       = 2,    // index page
    Expansion   = 3,    // overflow (future)
    FreeList    = 4,    // free list
    FreeSpace   = 5,    // Freespace map (future)
};

// sentinel values for invalid offsets
inline constexpr PageNum INVALID_PAGE = std::numeric_limits<PageNum>::max();
inline constexpr SlotNum INVALID_SLOT = std::numeric_limits<SlotNum>::max();

struct RID {
    PageNum page;
    SlotNum slot;

    bool operator==(const RID& other) const {
        return page == other.page && slot == other.slot;
    }
    bool operator!=(const RID& other) const {
        return !(*this == other);
    }
    bool operator<(const RID& other) const {
        return page < other.page || (page == other.page && slot < other.slot);
    }
    bool operator>(const RID& other) const {
        return page > other.page || (page == other.page && slot > other.slot);
    }
    bool operator<=(const RID& other) const {
        return !(*this > other);
    }
    bool operator>=(const RID& other) const {
        return !(*this < other);
    }
};

} // namespace LiliumDB

#endif