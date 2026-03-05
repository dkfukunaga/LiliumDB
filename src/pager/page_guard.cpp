#include "page_guard.h"

#include "pager.h"
#include "common/types.h"


namespace LiliumDB {

PageGuard::PageGuard(Pager* pager, PageNum pageNum, ByteSpan data)
    : pager_(pager)
    , pageNum_(pageNum)
    , data_(data) {
    pager_->pinPage(pageNum);
}

// Transfers existing pin; does not call pinPage.
PageGuard::PageGuard(PageGuard&& other) noexcept
    : pager_(other.pager_)
    , pageNum_(other.pageNum_)
    , data_(other.data_) {
    // invalidate other PageGuard
    other.pager_    = nullptr;
    other.pageNum_  = INVALID_PAGE;
    other.data_     = ByteSpan();
}

PageGuard::~PageGuard() {
    if (pager_) {
        pager_->unpinPage(pageNum());
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

ByteView PageGuard::view() const {
    assert(pager_ && "Cannot access page data from an invalid PageGuard");
    return ByteView(data_);
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

        // invalidate other PageGuard
        other.pager_    = nullptr;
        other.pageNum_  = INVALID_PAGE;
        other.data_     = ByteSpan();
    }
    return *this;
}

} // namespace LiliumDB