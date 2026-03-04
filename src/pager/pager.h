#ifndef LILIUMDB_PAGER_H
#define LILIUMDB_PAGER_H

#include "common/types.h"
#include "common/status.h"
#include "common/result.h"

namespace LiliumDB {

class PageGuard;

// Implementations should open the database file in their constructor.
// A successfully constructed Pager is ready to use.
class Pager {
public:
    // Implementations should close the file in their destructor if still open.
    virtual ~Pager() = default;

    virtual Status              close() = 0;
    virtual Result<PageGuard>   fetchPage(PageNum pageNum) = 0;
    virtual Result<PageGuard>   newPage(PageType type) = 0;
    virtual Status              deletePage(PageNum pageNum) = 0;
    virtual Status              flushPage(PageNum pageNum) = 0;
    virtual Status              flushAll() = 0;
    virtual bool                isOpen() const = 0;
private:
    friend class PageGuard;

    virtual void                markDirty(PageNum pageNum) noexcept = 0;
    virtual void                pinPage(PageNum pageNum) noexcept = 0;
    virtual void                unpinPage(PageNum pageNum) noexcept = 0;
};

} // namespace LiliumDB

#endif