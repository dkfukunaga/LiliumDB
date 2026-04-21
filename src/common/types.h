#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>
#include <limits>

namespace LiliumDB {

using PageNum       = uint32_t;
using PageOffset    = uint16_t;
using SlotIndex     = uint16_t;

// sentinel value for invalid page
inline constexpr PageNum INVALID_PAGE = std::numeric_limits<PageNum>::max();

// sentinel value for invalid slot
inline constexpr SlotIndex INVALID_SLOT = std::numeric_limits<SlotIndex>::max();

} // namespace LiliumDB

#endif