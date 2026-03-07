#ifndef LILIUMDB_PAGER_H
#define LILIUMDB_PAGER_H

#include "common/core.h"
#include "common/file_format.h"

namespace LiliumDB {

class PageGuard;

// Manages a buffer pool over a database file.
// Implementations should open the database file in their constructor;
// a successfully constructed Pager is ready to use.
// Page access is mediated through PageGuard, which pins pages in the buffer
// pool for the duration of its lifetime and unpins them on destruction.
class Pager {
public:
    // Implementations should close the file in their destructor if still open.
    virtual ~Pager() = default;

    // --- File Lifetime ---

    /// Returns true if the database file is open and ready for use.
    virtual bool isOpen() const = 0;

    /// Flushes all dirty pages and closes the database file.
    /// Implementations are already open on construction;
    /// call close() for clean shutdown.
    virtual VoidResult close() = 0;

    // --- Page Access ---

    /// Fetches the page with the given page number into the buffer pool.
    /// Returns a PageGuard pinning the page for the duration of its lifetime.
    /// Returns an error if the page does not exist or cannot be read.
    virtual DbResult<PageGuard> fetchPage(PageNum pageNum) = 0;

    /// Allocates a new page of the given type and loads it into the buffer pool.
    /// Returns a PageGuard pinning the new page for the duration of its lifetime.
    virtual DbResult<PageGuard> newPage(PageType type) = 0;

    /// Marks the given page as deleted and reclaims its page number for reuse.
    virtual VoidResult deletePage(PageNum pageNum) = 0;

    // --- Durability ---

    /// Flushes the given dirty page to disk immediately.
    /// Has no effect if the page is not dirty or not in the buffer pool.
    virtual VoidResult flushPage(PageNum pageNum) = 0;

    /// Flushes all dirty pages in the buffer pool to disk.
    virtual VoidResult flushAll() = 0;
private:
    // PageGuard calls these directly to manage pin state and dirty tracking.
    friend class PageGuard;

    virtual void markDirty(PageNum pageNum) noexcept = 0;
    virtual void pinPage(PageNum pageNum) noexcept = 0;
    virtual void unpinPage(PageNum pageNum) noexcept = 0;
};

} // namespace LiliumDB

#include "page_guard.h" // PageGuard needs Pager to be defined first

#endif