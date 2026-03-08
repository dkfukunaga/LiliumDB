#ifndef LILIUMDB_PAGE_GUARD_H
#define LILIUMDB_PAGE_GUARD_H

#include "common/types.h"
#include "utils/byte_span.h"
#include "pin_manager.h"

namespace LiliumDB {

class Pager;

// RAII handle to a page pinned in the buffer pool.
// Pins the page on construction and unpins on destruction.
// Move-only: copying would result in double-unpin on destruction.
// Do not hold ByteSpan or ByteView past the lifetime of the guard.
class PageGuard {
public:
    PageGuard() : pager_(nullptr), pageNum_(INVALID_PAGE), data_(), dirty_(false) { }
    /// Pins the given page in the buffer pool.
    PageGuard(PinManager* pager, PageNum pageNum, ByteSpan data);
    PageGuard(const PageGuard&) = delete;
    /// Transfers ownership of the pin; the moved-from guard becomes invalid.
    PageGuard(PageGuard&& other) noexcept;
    /// Unpins the page, making it eligible for eviction.
    ~PageGuard();

    /// Returns a mutable span over the page data and marks the page dirty.
    ByteSpan    span();
    /// Returns a mutable span over a region the page data and marks the page dirty.
    /// ByteSpan throws std::out_of_range.
    ByteSpan    subspan(PageOffset start, uint16_t len);
    /// Returns a read-only view over the page data.
    ByteView    view() const;
    /// Returns a read-only view over a region the page data.
    /// ByteView throws std::out_of_range.
    ByteView    subview(PageOffset start, uint16_t len) const;
    PageNum     pageNum() const { return pageNum_; }

    PageGuard&  operator=(const PageGuard&) = delete;
    /// Unpins the current page and transfers ownership of the incoming pin.
    PageGuard&  operator=(PageGuard&& other) noexcept;
private:
    PinManager* pager_;
    PageNum     pageNum_;
    ByteSpan    data_;
    bool        dirty_;

    void        invalidate() noexcept;
};

} // namespace LiliumDB

#endif