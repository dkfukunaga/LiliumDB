#ifndef LILIUMDB_BTREE_TYPES_H
#define LILIUMDB_BTREE_TYPES_H

#include "common/types.h"

#include <vector>

namespace LiliumDB {

// Record identifier: a stable reference to a record by its page and slot.
// A default-constructed RID is invalid; use isValid() to check before use.
// Ordering is by page first, then slot within page.
struct RID {
    PageNum page = INVALID_PAGE;
    SlotIndex slot = INVALID_SLOT;

    constexpr RID() noexcept = default;
    constexpr RID(PageNum pn, SlotIndex sn) noexcept: page(pn), slot(sn) { }

    // Returns false if either component holds its sentinel value.
    constexpr bool isValid() const noexcept {
        return page != INVALID_PAGE && slot != INVALID_SLOT;
    }

    friend constexpr bool operator==(const RID& lhs, const RID& rhs) noexcept {
        return lhs.page == rhs.page && lhs.slot == rhs.slot;
    }
    friend constexpr bool operator!=(const RID& lhs, const RID& rhs) noexcept {
        return !(lhs == rhs);
    }
    friend constexpr bool operator<(const RID& lhs, const RID& rhs) noexcept {
        return lhs.page < rhs.page || (lhs.page == rhs.page && lhs.slot < rhs.slot);
    }
    friend constexpr bool operator>(const RID& lhs, const RID& rhs) noexcept {
        return lhs.page > rhs.page || (lhs.page == rhs.page && lhs.slot > rhs.slot);
    }
    friend constexpr bool operator<=(const RID& lhs, const RID& rhs) noexcept {
        return !(lhs > rhs);
    }
    friend constexpr bool operator>=(const RID& lhs, const RID& rhs) noexcept {
        return !(lhs < rhs);
    }
};

struct Slot {
    PageOffset  offset;
    RecordSize  size;
};

using Record = std::vector<uint8_t>;

} // namespace LiliumDB

#endif