#ifndef LILIUMDB_CURSOR_H
#define LILIUMDB_CURSOR_H

#include "common/core.h"
#include "pager/pager.h"
#include "pager/page_guard.h"
#include "utils/byte_span.h"
#include "btree_page.h"

namespace LiliumDB {

using namespace LiliumDB::BTreePage;

class Cursor {
public:
    Cursor(Pager* pager, PageGuard page, SlotIndex slot)
        : pager_(pager)
        , page_(std::move(page))
        , slot_(slot)
        , valid_(true) {
        if (slot == INVALID_SLOT)
            invalidate();
    }
    Cursor() : pager_(nullptr), page_(PageGuard()), slot_(INVALID_SLOT), valid_(false) { }
    Cursor(const Cursor&) = delete;
    Cursor(Cursor&& other) noexcept
        : pager_(other.pager_)
        , page_(std::move(other.page_))
        , slot_(other.slot_)
        , valid_(other.valid_) {
        other.invalidate();
    }
    ~Cursor() = default;

    ByteView        key()   const { return valid_? getKey(page_, slot_) : ByteView(); }
    ByteView        value() const { return valid_? getValue(page_, slot_) : ByteView(); }
    bool            valid() const { return valid_; }

    DbResult<void>  next();
    DbResult<void>  prev();

private:
    Pager*          pager_;
    PageGuard       page_;
    SlotIndex       slot_;
    bool            valid_;

    void            invalidate() {
        page_ = PageGuard();
        slot_ = INVALID_SLOT;
        valid_ = false;
    }
};

} // namespace LiliumDB

#endif