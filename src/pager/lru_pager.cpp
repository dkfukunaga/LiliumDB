#include "lru_pager.h"

#include "storage/std_page_io.h"
#include "utils/byte_span_macros.h"

#include <cstddef>
#include <cstring>
#include <ctime>

namespace LiliumDB {

DbResult<std::unique_ptr<Pager>>
LRUPager::open(std::string_view path, OpenMode mode, size_t poolSize) {
    std::unique_ptr<PageIO> pio;
    ASSIGN_OR_RETURN(pio, StdPageIO::open(path, mode));

    std::unique_ptr<LRUPager> lruPager(new LRUPager(std::move(pio), mode, poolSize));

    if (lruPager->pageIO_->pageCount() == 0) { // new uninitialized file
        RETURN_ON_ERROR(lruPager->initFile());
    } else {
        RETURN_ON_ERROR(lruPager->validateFileHeader());
    }

    std::unique_ptr<Pager> pager = std::move(lruPager);
    return Ok(std::move(pager));
}

DbResult<void> LRUPager::close() {
    for (Frame frame : frames_) {
        assert(frame.pinCount == 0);
    }

    if (openMode_ != OpenMode::ReadOnly) {
        RETURN_ON_ERROR(flushAll());
    }

    RETURN_ON_ERROR(pageIO_->close());

    return Ok();
}

DbResult<PageGuard> LRUPager::fetchPage(PageNum pageNum) {
    if (pageNum > highestAllocated_) {
        return Err(Status::invalidArg("Page out of range."));
    }

    // check if page is already in buffer pool
    auto mapIter = pageMap_.find(pageNum);
    if (mapIter != pageMap_.end()) {
        auto frameIter = mapIter->second;
        lruList_.splice(lruList_.begin(), lruList_, frameIter);
        Frame& frame = frames_[*frameIter];

        return Ok(PageGuard(this, pageNum, frame.span));
    }

    // else, allocate a new frame and read page from disk
    FrameIndex frameIndex;
    ASSIGN_OR_RETURN(frameIndex, allocateFrame(pageNum));
    Frame& frame = frames_[frameIndex];
    RETURN_ON_ERROR(pageIO_->readPage(pageNum, frame.span));

    // after CRC32 is implemented, calculate checksum and compare to checksum on page
    // if different, flag as corrupt and return an error code.

    return Ok(PageGuard(this, pageNum, frame.span));
}

DbResult<PageGuard> LRUPager::newPage(PageType pageType) {
    PageNum pageNum;
    PageGuard page;

    if (freespaceHead_ != INVALID_PAGE) {
        pageNum = freespaceHead_;
        ASSIGN_OR_RETURN(page, fetchPage((pageNum)));

        // reset freespace head
        auto header = page.getHeader();
        freespaceHead_ = header.next;

        // re-initialize page
        ASSIGN_OR_RETURN(page, initPage(std::move(page), pageType));
    } else {
        // get next page number
        pageNum = ++highestAllocated_;

        // allocate a frame in the buffer pool
        FrameIndex index;
        ASSIGN_OR_RETURN(index, allocateFrame(pageNum));
        Frame& frame = frames_[index];

        // contruct PageGuard to send to initPage
        page = PageGuard(this, pageNum, frame.span);
        ASSIGN_OR_RETURN(page, initPage(std::move(page), pageType));
    }

    return Ok(std::move(page));
}

DbResult<void> LRUPager::deletePage(PageNum pageNum) {
    // check for page 0
    if (pageNum == 0) {
        return Err(Status::invalidArg("Cannot delete page 0."));
    }

    // check if page is pinned
    Frame& frame = frames_[*pageMap_[pageNum]];
    if (frame.pinCount > 0) {
        return Err(Status::invalidArg("Page " + std::to_string(pageNum) + " is pinned."));
    }

    // get page to be deleted
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(pageNum));

    // cache old freespace head and set new one
    PageNum nextFree = freespaceHead_;
    freespaceHead_ = pageNum;

    // update page header
    auto header = page.getHeader();

    header.pageType = PageType::FreeList;
    header.next = nextFree;

    page.setHeader(header);

    return Ok();
}

DbResult<void> LRUPager::flushPage(PageNum pageNum) {
    if (pageNum >= appendStart_) {
        while (pageNum >= appendStart_) {
            RETURN_ON_ERROR(flush(appendStart_++));
        }
    } else {
        RETURN_ON_ERROR(flush(pageNum));
    }

    return Ok();
}

DbResult<void> LRUPager::flushAll() {
    RETURN_ON_ERROR(updateFileHeader());
    for (PageNum i = 0; i <= highestAllocated_; ++i) {
        if (pageMap_.find(i) != pageMap_.end()) {
            RETURN_ON_ERROR(flush(i));
            if (i == appendStart_) {
                ++appendStart_;
            }
        }
    }

    return Ok();
}

void LRUPager::markDirty(PageNum pageNum) noexcept {
    assert(pageMap_.find(pageNum) != pageMap_.end());

    FrameIndex frameIndex = *pageMap_[pageNum];
    frames_[frameIndex].dirty = true;
}

void LRUPager::pinPage(PageNum pageNum) noexcept {
    assert(pageMap_.find(pageNum) != pageMap_.end());

    FrameIndex frameIndex = *pageMap_[pageNum];
    frames_[frameIndex].pinCount++;
}

void LRUPager::unpinPage(PageNum pageNum) noexcept {
    assert(pageMap_.find(pageNum) != pageMap_.end());

    FrameIndex frameIndex = *pageMap_[pageNum];
    assert(frames_[frameIndex].pinCount > 0);
    frames_[frameIndex].pinCount--;
}

DbResult<void> LRUPager::validateFileHeader() {
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(0));
    ByteView headerView = page.subview(0, sizeof(FileHeader));

    // check magic bytes
    uint8_t mBytes[MAGIC_BYTES_LEN];
    headerView.read(offsetof(FileHeader, magicBytes), mBytes, MAGIC_BYTES_LEN);
    if (memcmp(mBytes, MAGIC_BYTES, MAGIC_BYTES_LEN) != 0) {
        return Err(Status::fileInvalid("Incorrect file type."));
    }

    // check for corrupt flag
    FileFlags flags = headerView.get<FileFlags>(offsetof(FileHeader, fileFlags));
    if (flags.has(FileFlag::Corrupt)) {
        return Err(Status::corrupt("File is corrupt."));
    }

    // check major version
    uint8_t vMajor = headerView.get<uint8_t>(offsetof(FileHeader, versionMajor));
    if (vMajor != VERSION_MAJOR) {
        return Err(Status::fileInvalid("Incompatible file version."));
    }

    // read metadata
    freespaceHead_ = headerView.get<PageNum>(offsetof(FileHeader, freespaceHead));
    appendStart_ = headerView.get<PageNum>(offsetof(FileHeader, pageCount));
    highestAllocated_ = appendStart_ == 0 ? INVALID_PAGE : appendStart_ - 1;

    return Ok();
}

DbResult<void> LRUPager::initFile() {
    // initialize metadata
    appendStart_ = 1;
    highestAllocated_ = 0;
    freespaceHead_ = INVALID_PAGE;

    // allocate new page zero
    RETURN_ON_ERROR(allocateFrame(0));

    // get PageGuard for page 0
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(0));

    // create a new file header
    int64_t now = static_cast<int64_t>(std::time(nullptr));

    FileHeader header;
    header.pageCount = 1;
    header.fileCreated = now;
    header.lastModified = now;

    // serialize header
    page.span().put<FileHeader>(0, header);

    // initialize page 0 to a Table page
    RETURN_ON_ERROR(initPage(std::move(page), PageType::Table));

    return Ok();
}

DbResult<PageGuard> LRUPager::initPage(PageGuard&& page, PageType pageType) {// zero-initialize page
    auto offset = page.pageOffset();
    page.subspan(offset, PAGE_SIZE - offset).clear();

    // write new page header
    PageHeader header;

    header.pageType = pageType;
    header.freeOffset = PAGE_HEADER_SIZE + page.pageOffset();
    page.setHeader(header);

    // write new page footer
    PageFooter footer;
    // calculate CRC32 after checksumming is implemented
    page.span().put<PageFooter>(PAGE_END_OFFSET, footer);

    return Ok(std::move(page));
}

DbResult<LRUPager::FrameIndex> LRUPager::allocateFrame(PageNum pageNum) {
    FrameIndex index;
    if (nextFreeFrame_ < frames_.size()) {
        index = nextFreeFrame_++;
    } else {
        ASSIGN_OR_RETURN(index, evictLastUsedPage());
    }

    frames_[index].pageNum = pageNum;

    auto iter = lruList_.emplace(lruList_.begin(), index);
    pageMap_.insert({pageNum, iter});

    return Ok(index);
}

DbResult<LRUPager::FrameIndex> LRUPager::evictLastUsedPage() {
    auto iter = lruList_.end();

    while (iter != lruList_.begin()) {
        --iter;
        if (frames_[*iter].pinCount == 0) {
            break;
        }
    }
    if (iter == lruList_.begin() && frames_[*iter].pinCount != 0) {
        return Err(Status::bufferFull("Buffer pool is in use."));
    }

    Frame& frame = frames_[*iter];
    if (frame.dirty == true) {
        RETURN_ON_ERROR(flush(frame.pageNum));
    }

    FrameIndex index = *iter;
    pageMap_.erase(frame.pageNum);
    lruList_.erase(iter);

    return Ok(index);
}

DbResult<void> LRUPager::flush(PageNum pageNum) {
    if (openMode_ == OpenMode::ReadOnly) {
        return Err(Status::fileErr("File is read-only."));
    }

    FrameIndex frameIndex = *pageMap_[pageNum];
    Frame& frame = frames_[frameIndex];

    if (frame.dirty) {
        // after CRC32 is implemented, recalculate checksum and write to footer
        // before writing to disk.

        RETURN_ON_ERROR(pageIO_->writePage(pageNum, frame.span));
        frame.dirty = false;
    }

    return Ok();
}

DbResult<void> LRUPager::updateFileHeader() {
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(0));

    ByteSpan headerSpan = page.subspan(0, FILE_HEADER_SIZE);

    int64_t now = static_cast<int64_t>(std::time(nullptr));
    uint32_t pageCount = highestAllocated_ == INVALID_PAGE ? 0 : highestAllocated_ + 1;

    headerSpan.put<uint32_t>(offsetof(FileHeader, pageCount), pageCount);
    headerSpan.put<int64_t>(offsetof(FileHeader, lastModified), now);
    headerSpan.put<PageNum>(offsetof(FileHeader, freespaceHead), freespaceHead_);
    // checksum placeholder
    // headerSpan.put<uint32_t>(offsetof(FileHeader, checksum), CHECKSUM_PLACEHOLDER);

    return Ok();
}

} // namespace LiliumDB