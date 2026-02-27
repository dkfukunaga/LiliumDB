#ifndef LILIUMDB_COMMON_H
#define LILIUMDB_COMMON_H

#include <cstdint>
#include <limits>

namespace LiliumDB {

using PageNum = uint32_t;
using SlotNum = uint16_t;

inline constexpr PageNum INVALID_PAGE = std::numeric_limits<uint32_t>::max();
inline constexpr SlotNum INVALID_SLOT = std::numeric_limits<uint16_t>::max();
inline constexpr uint16_t PAGE_SIZE = 4096;

struct RID {
    PageNum page;
    SlotNum slot;
};

} // namespace LiliumDB


#endif