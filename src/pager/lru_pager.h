#ifndef LILIUMDB_LRU_PAGER_H
#define LILIUMDB_LRU_PAGER_H

#include "pager.h"

#include "common/core.h"
#include "common/file_format.h"
#include "common/byte_span.h"
#include "storage/page_io.h"
#include "pager/page_guard.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <list>
#include <string_view>

namespace LiliumDB {

class LRUPager : public Pager {
public:
    static constexpr size_t DEFAULT_POOL_SIZE = 64;

    static DbResult<std::unique_ptr<Pager>>
        open(std::string_view path, OpenMode mode, size_t poolSize = DEFAULT_POOL_SIZE);
    ~LRUPager() noexcept { if (isOpen()) close(); } // errors silently discarded on destruction

    bool                isOpen() const override { return pageIO_->isOpen(); }
    VoidResult          close() override;

    DbResult<PageGuard>   fetchPage(PageNum pageNum) override;
    DbResult<PageGuard>   newPage(PageType type) override;
    VoidResult              deletePage(PageNum pageNum) override;

    VoidResult              flushPage(PageNum pageNum) override;
    VoidResult              flushAll() override;

private:
    using FrameIndex = size_t;
    using FrameIter = std::list<FrameIndex>::iterator;

    struct Frame {
        ByteSpan    data;                       // points into pool_ at frame offset
        PageNum     pageNum  = INVALID_PAGE;    // which page is loaded, INVALID_PAGE if empty
        uint32_t    pinCount = 0;               // count of PageGuards accessing page
        bool        dirty    = false;           // flush on eviction if dirty

        Frame() = default;
        Frame(std::vector<uint8_t>& dat, FrameIndex idx, PageNum pn)
            : data(ByteSpan(&dat.data()[idx * PAGE_SIZE], PAGE_SIZE))
            , pageNum(pn) { }
    };

    std::unique_ptr<PageIO> pageIO_;
    PageNum freespaceHead_;
    PageNum appendStart_;
    FrameIndex nextFreeFrame_ = 0;

    // buffer pool of poolSize * PAGE_SIZE bytes
    std::vector<uint8_t> pool_;
    // vector of frames that point into pool_
    std::vector<Frame> frames_;
    // linked list of frame indices to keep track of most recently used page
    // any time a page is accessed, it is moved to the front of the list using
    // the iterator from pageMap_
    std::list<FrameIndex> lruList_;
    // map page number to frame index
    std::unordered_map<PageNum, FrameIter> pageMap_;

    LRUPager(std::unique_ptr<PageIO> pageIO, size_t poolSize)
        : pageIO_(std::move(pageIO))
        , pool_(poolSize * PAGE_SIZE)
        , frames_(poolSize) { }

    VoidResult              validateFileHeader();
    VoidResult              initFile();
    DbResult<FrameIndex>    allocatePage(PageNum pageNum);
    DbResult<FrameIndex>    evictLastUsedPage();
    VoidResult              serializeFileHeader(FileHeader header);
    DbResult<FileHeader>    deserializeFileHeader();

    void markDirty(PageNum pageNum) noexcept override;
    void pinPage(PageNum pageNum) noexcept override;
    void unpinPage(PageNum pageNum) noexcept override;
};

} // namespace LiliumDB

#endif