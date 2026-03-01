#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>
#include <string>

namespace LiliumDB {

using PageNum = uint32_t;
using SlotNum = uint16_t;
using FileOffset = uint64_t;
using PageOffset = uint16_t;

struct RID {
    PageNum page;
    SlotNum slot;

    bool operator==(const RID& other) const {
        return this->page == other.page && this->slot == other.slot;
    }
    bool operator!=(const RID& other) const {
        return !(*this == other);
    }
};

} // namespace LiliumDB

#endif