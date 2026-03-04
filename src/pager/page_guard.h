#ifndef LILIUMDB_PAGE_GUARD_H
#define LILIUMDB_PAGE_GUARD_H

#include "common/types.h"
#include "common/byte_span.h"

namespace LiliumDB {

class Pager;

// Pins page on creation and unpins on destruction
class PageGuard {
public:
    PageGuard(Pager* pager, PageNum pageNum, ByteSpan data);
    PageGuard(const PageGuard&) = delete;
    PageGuard(PageGuard&& other) noexcept;
    ~PageGuard();

    // automatically mark dirty on return of a mutable span
    ByteSpan    span();
    // do not mark dirty on return of immutable view
    ByteView    view() const;
    PageNum     pageNum() const { return pageNum_; }

    // no copy assignment; move only
    PageGuard&  operator=(const PageGuard&) = delete;
    PageGuard&  operator=(PageGuard&& other) noexcept;
private:
    Pager*      pager_;
    PageNum     pageNum_;
    ByteSpan    data_;
};

} // namespace LiliumDB

#endif