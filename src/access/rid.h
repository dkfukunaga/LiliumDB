#ifndef LILIUMDB_RID_H
#define LILIUMDB_RID_H

#include "common/types.h"

namespace LiliumDB {

// Record identifier: a stable reference to a record by its page and slot.
// A default-constructed RID is invalid; use isValid() to check before use.
// Ordering is by page first, then slot within page.
struct RID {
    PageNum page = INVALID_PAGE;
    SlotNum slot = INVALID_SLOT;

    constexpr RID() noexcept = default;
    constexpr RID(PageNum pn, SlotNum sn) noexcept: page(pn), slot(sn) { }

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

} // namespace LiliumDB

#endif