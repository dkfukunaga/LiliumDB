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

    std::unique_ptr<LRUPager> lruPager(new LRUPager(std::move(pio), poolSize));

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

    RETURN_ON_ERROR(updateFileHeader());
    RETURN_ON_ERROR(flushAll());
    RETURN_ON_ERROR(pageIO_->close());

    return Ok();
}

DbResult<PageGuard> LRUPager::fetchPage(PageNum pageNum) {
    if (pageNum > highestAllocated_) {
        return Err(Status::fileErr("Page out of range."));
    }

    // check if page is already in buffer pool
    auto mapIter = pageMap_.find(pageNum);
    if (mapIter != pageMap_.end()) {
        auto frameIter = mapIter->second;
        lruList_.splice(lruList_.begin(), lruList_, frameIter);
        Frame& frame = frames_[*frameIter];

        return Ok(PageGuard(this, pageNum, frame.pageType, frame.span));
    }

    // else, allocate a new frame and read page from disk
    FrameIndex frameIndex;
    ASSIGN_OR_RETURN(frameIndex, allocateFrame(pageNum));
    Frame& frame = frames_[frameIndex];
    RETURN_ON_ERROR(pageIO_->readPage(pageNum, frame.span));

    // read page type and set in Frame
    PageOffset pageOffset = pageNum == 0 ? FILE_HEADER_SIZE : 0;
    ByteView view = frame.span.subview(pageOffset, PAGE_HEADER_SIZE);
    frame.pageType = view.get<PageType>(offsetof(PageHeader, pageType));

    return Ok(PageGuard(this, pageNum, frame.pageType, frame.span));
}

DbResult<PageGuard> LRUPager::newPage(PageType pageType) {
    PageNum pageNum;

    if (highestAllocated_ == INVALID_PAGE) {
        appendStart_ = 1;
        highestAllocated_ = 0;
        pageNum = 0;
    } else {
        pageNum = ++highestAllocated_;
        RETURN_ON_ERROR(allocateFrame(pageNum));
    }

    PageGuard pageGuard;
    ASSIGN_OR_RETURN(pageGuard, initPage(pageNum, pageType));

    return Ok(std::move(pageGuard));
}

DbResult<void> LRUPager::deletePage(PageNum pageNum) {

    return Err(Status::error("Page deletion not implemented yet."));
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
    if (highestAllocated_ == INVALID_PAGE) {
        RETURN_ON_ERROR(flush(0));
    } else {
        for (PageNum i = 0; i <= highestAllocated_; ++i) {
            if (pageMap_.find(i) != pageMap_.end()) {
                RETURN_ON_ERROR(flush(i));
                if (i == appendStart_) {
                    ++appendStart_;
                }
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
    PageGuard pageGuard;
    ASSIGN_OR_RETURN(pageGuard, fetchPage(0));
    ByteView headerView = pageGuard.subview(0, sizeof(FileHeader));

    // check magic bytes
    constexpr size_t mBytesLen = sizeof(FileHeader::magicBytes);
    uint8_t mBytes[mBytesLen];
    headerView.read(offsetof(FileHeader, magicBytes), mBytes, mBytesLen);
    if (memcmp(mBytes, MAGIC_BYTES.data(), mBytesLen) != 0) {
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

DbResult<void> LRUPager::initFile() { // initialize new file header
    FileHeader header;

    int64_t now = static_cast<int64_t>(std::time(nullptr));

    std::memcpy(header.magicBytes, MAGIC_BYTES.data(), sizeof(header.magicBytes));
    header.fileFlags = FileFlags();
    header.versionMajor = VERSION_MAJOR;
    header.versionMinor = VERSION_MINOR;
    header.pageCount = 0;   // page 0 has not been initialized
    header.fileCreated = now;
    header.lastModified = now;
    header.freespaceHead = INVALID_PAGE;
    // skip reserved bytes
    header.checksum = CHECKSUM_PLACEHOLDER;

    // allocate new page zero
    RETURN_ON_ERROR(allocateFrame(0));

    // get PageGuard for page 0
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(0));

    // serialize header
    ByteSpan headerSpan = page.subspan(0, sizeof(FileHeader));

    headerSpan.write(0, header.magicBytes, sizeof(header.magicBytes));
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, fileFlags);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, versionMajor);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, versionMinor);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, pageCount);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, fileCreated);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, lastModified);
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, freespaceHead);
    // skip reserved bytes
    SPAN_WRITE_STRUCT_FIELD(headerSpan, header, checksum);

    // initialize metadata
    appendStart_ = 0;
    highestAllocated_ = INVALID_PAGE;
    freespaceHead_ = INVALID_PAGE;

    return Ok();
}

DbResult<PageGuard> LRUPager::initPage(PageNum pageNum, PageType pageType) {
    PageOffset pageOffset = pageNum == 0 ? FILE_HEADER_SIZE : 0;

    // initialize new page header
    PageHeader header;

    header.pageType = pageType;
    header.pageFlags = PageFlags();
    header.level = INVALID_PAGE_LEVEL;
    // skip reserved byte
    header.slotCount = 0;
    header.freeOffset = PAGE_HEADER_SIZE + pageOffset;
    header.next = INVALID_PAGE;
    header.prev = INVALID_PAGE;
    header.checksum = CHECKSUM_PLACEHOLDER;

    // get Frame and set page type
    Frame& frame = frames_[*pageMap_[pageNum]];
    frame.pageType = pageType;

    // get PageGuard for page
    PageGuard page;
    ASSIGN_OR_RETURN(page, fetchPage(pageNum));

    // serialize page header
    ByteSpan pageHeaderSpan = page.subspan(pageOffset, PAGE_HEADER_SIZE);

    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, pageType);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, pageFlags);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, level);
    // skip reserved byte
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, slotCount);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, freeOffset);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, next);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, prev);
    SPAN_WRITE_STRUCT_FIELD(pageHeaderSpan, header, checksum);

    return Ok(std::move(page));
}

DbResult<LRUPager::FrameIndex> LRUPager::allocateFrame(PageNum pageNum, PageType pageType) {
    FrameIndex index;
    if (nextFreeFrame_ < frames_.size()) {
        index = nextFreeFrame_++;
    } else {
        ASSIGN_OR_RETURN(index, evictLastUsedPage());
    }

    frames_[index] = Frame(pool_, index, pageNum, pageType);
    auto iter = lruList_.emplace(lruList_.begin(), index);
    pageMap_.insert({pageNum, iter});

    return Ok(index);
}

DbResult<LRUPager::FrameIndex> LRUPager::evictLastUsedPage() {


    return Err(Status::error("Eviction not implemented yet."));
}

DbResult<void> LRUPager::flush(PageNum pageNum) {
    FrameIndex frameIndex = *pageMap_[pageNum];
    Frame& frame = frames_[frameIndex];

    if (frame.dirty) {
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