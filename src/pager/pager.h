#ifndef LILIUMDB_PAGER_H
#define LILIUMDB_PAGER_H

#include "common/types.h"
#include "common/status.h"
#include "common/byte_span.h"
#include "common/result.h"

#include <string_view>

namespace LiliumDB {

class Pager {
public:
    class PageGuard {
    public:
        ByteSpan&       span()       { return data_; }
        const ByteSpan& span() const { return data_; }
        PageNum         pageNum() const { return pageNum_; }

        void            markDirty() noexcept { dirty_ = true; }

        // move only
        PageGuard(const PageGuard&) = delete;
        PageGuard& operator=(const PageGuard&) = delete;
        PageGuard(PageGuard&& other) noexcept
            : pager_(other.pager_), pageNum_(other.pageNum_), data_(other.data_) {
            other.pager_ = nullptr;
        }
        PageGuard& operator=(PageGuard&& other) noexcept {
            if (this != &other) {
                if (pager_) pager_->unpinPage(pageNum_, dirty_);  // release current page
                pager_    = other.pager_;
                pageNum_  = other.pageNum_;
                data_     = other.data_;
                other.pager_ = nullptr;
            }
            return *this;
        }

        // RAII destructor
        ~PageGuard() { if (pager_) pager_->unpinPage(pageNum_, dirty_); }
    private:
        friend class Pager;
        PageGuard(Pager* pager, PageNum pageNum, ByteSpan data):
            pager_(pager), pageNum_(pageNum), data_(data), dirty_(false) { }

        Pager*      pager_;
        PageNum     pageNum_;
        ByteSpan    data_;
        bool        dirty_;
    };

    virtual Status              open(std::string_view path, OpenMode mode = OpenMode::ReadWrite) = 0;
    virtual Status              close() = 0;
    virtual Result<PageGuard>   fetchPage(PageNum pageNum) = 0;
    virtual Result<PageNum>     allocatePage() = 0;
    virtual bool                isOpen() const = 0;
    virtual bool                isNewFile() const = 0;

    virtual ~Pager() = default;
protected:
    PageGuard makeGuard(PageNum pageNum, ByteSpan data) noexcept {
        return PageGuard(this, pageNum, data);
    }
    virtual void unpinPage(PageNum pageNum, bool dirty) noexcept = 0;
};


} // namespace LiliumDB

#endif