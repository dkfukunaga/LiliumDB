#ifndef LILIUMDB_CURSOR_H
#define LILIUMDB_CURSOR_H

#include "common/core.h"
#include "pager/pager.h"
#include "pager/page_guard.h"
#include "utils/byte_span.h"
#include "btree_page.h"

namespace LiliumDB {

class Cursor {
public:
    Cursor(Pager* pager, PageGuard page, SlotIndex slot)
        : pager_(pager)
        , page_(std::move(page))
        , slot_(slot) { }
    Cursor() : pager_(nullptr), page_(PageGuard()), slot_(INVALID_SLOT), valid_(false) { }
    Cursor(const Cursor&) = delete;
    Cursor(Cursor&& other) noexcept;
    ~Cursor() = default;

    ByteView        key() const;
    ByteView        value() const;
    bool            valid() const { return valid_; }

    DbResult<void>  next();
    DbResult<void>  prev();

private:
    Pager*      pager_;
    PageGuard   page_;
    SlotIndex   slot_;
    bool        valid_;

    void        invalidate() {
        slot_ = INVALID_SLOT;
        valid_ = false;
    }
};

} // namespace LiliumDB

#endif