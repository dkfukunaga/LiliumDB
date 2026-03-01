#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>

namespace LiliumDB {

using PageNum = uint32_t;
using SlotNum = uint16_t;
using FileOffset = uint64_t;
using PageOffset = uint16_t;

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