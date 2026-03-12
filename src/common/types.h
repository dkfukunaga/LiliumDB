#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>
#include <limits>

namespace LiliumDB {

using PageNum       = uint32_t;
using SlotIndex     = uint16_t;
using PageOffset    = uint16_t;
using RecordSize    = uint16_t;

// sentinel values for invalid offsets
inline constexpr PageNum INVALID_PAGE = std::numeric_limits<PageNum>::max();
inline constexpr SlotIndex INVALID_SLOT = std::numeric_limits<SlotIndex>::max();

} // namespace LiliumDB

#endif