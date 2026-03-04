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

PageGuard::PageGuard(PageGuard&& other) noexcept
    : pager_(other.pager_)
    , pageNum_(other.pageNum_)
    , data_(other.data_) {
    // invalidate other PageGuard
    other.pager_ = nullptr;
    other.pageNum_  = INVALID_PAGE;
    other.data_     = ByteSpan(nullptr, 0);
}

PageGuard::~PageGuard() {
    if (pager_) {
        pager_->unpinPage(pageNum());
    }
}

ByteSpan PageGuard::span() {
    if (pager_) { 
        pager_->markDirty(pageNum());
    }
    return data_;
}

ByteView PageGuard::view() const {
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
        other.data_     = ByteSpan(nullptr, 0);
    }
    return *this;
}

} // namespace LiliumDB