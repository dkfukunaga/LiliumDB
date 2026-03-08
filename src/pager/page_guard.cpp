#include "page_guard.h"

#include "pager.h"
#include "common/types.h"

#include <cassert>

namespace LiliumDB {

PageGuard::PageGuard(Pager* pager, PageNum pageNum, ByteSpan data)
    : pager_(pager)
    , pageNum_(pageNum)
    , data_(data)
    , dirty_(false) {
    pager_->pinPage(pageNum);
}

PageGuard::PageGuard(PageGuard&& other) noexcept
    : pager_(other.pager_)
    , pageNum_(other.pageNum_)
    , data_(other.data_)
    , dirty_(other.dirty_) {
    // invalidate other PageGuard
    other.invalidate();
}

PageGuard::~PageGuard() {
    if (pager_) {
        pager_->unpinPage(pageNum_);
    }
}

ByteSpan PageGuard::span() {
    assert(pager_ && "Cannot access page data from an invalid PageGuard");
    if (!dirty_) { 
        pager_->markDirty(pageNum_);
        dirty_ = true;
    }
    return data_;
}

ByteSpan PageGuard::subspan(PageOffset start, uint16_t len) {
    assert(pager_ && "Cannot access page data from an invalid PageGuard");
    if (!dirty_) { 
        pager_->markDirty(pageNum_);
        dirty_ = true;
    }
    return data_.subspan(start, len);
}

ByteView PageGuard::view() const {
    assert(pager_ && "Cannot access page data from an invalid PageGuard");
    return ByteView(data_);
}

ByteView PageGuard::subview(PageOffset start, uint16_t len) const {
    assert(pager_ && "Cannot access page data from an invalid PageGuard");
    return data_.subview(start, len);
}

PageGuard& PageGuard::operator=(PageGuard&& other) noexcept {
    if (this != &other) {
        // release current page
        if (pager_) {
            pager_->unpinPage(pageNum_);
        }

        // move ownership from other PageGuard
        pager_      = other.pager_;
        pageNum_    = other.pageNum_;
        data_       = other.data_;
        dirty_      = other.dirty_;

        // invalidate other PageGuard
        other.invalidate();
    }
    return *this;
}

void PageGuard::invalidate() noexcept {
    pager_ = nullptr;
    pageNum_ = INVALID_PAGE;
    data_ = ByteSpan();
    dirty_ = false;
}

} // namespace LiliumDB