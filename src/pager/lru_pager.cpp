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

    RETURN_ON_ERROR(flushAll());
    RETURN_ON_ERROR(pageIO_->close());

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

    return Ok();
}

DbResult<void> LRUPager::initFile() {
    // initialize new file header
    FileHeader header;

    int64_t now = static_cast<int64_t>(std::time(nullptr));

    std::memcpy(header.magicBytes, MAGIC_BYTES.data(), sizeof(header.magicBytes));
    header.fileFlags = FileFlags();
    header.versionMajor = VERSION_MAJOR;
    header.versionMinor = VERSION_MINOR;
    header.pageCount = 1;
    header.fileCreated = now;
    header.lastModified = now;
    header.freespaceHead = INVALID_PAGE;
    header.checksum = CHECKSUM_PLACEHOLDER;

    // allocate new page zero
    RETURN_ON_ERROR(allocatePage(0));

    // write file header to page zero
    RETURN_ON_ERROR(serializeFileHeader(header));

    // initialize metadata
    appendStart_ = 1;
    freespaceHead_ = INVALID_PAGE;

    return Ok();
}

DbResult<LRUPager::FrameIndex> LRUPager::allocatePage(PageNum pageNum) {
    FrameIndex index;
    if (nextFreeFrame_ < frames_.size()) {
        index = nextFreeFrame_++;
    } else {
        ASSIGN_OR_RETURN(index, evictLastUsedPage());
    }
    Frame frame(pool_, index, pageNum);

    frames_[index] = frame;
    auto iter = lruList_.emplace(lruList_.begin(), index);
    pageMap_.insert({pageNum, iter});

    if (pageNum > highestAllocated_) {
        ++highestAllocated_;
    }

    return Ok(index);
}

DbResult<void> LRUPager::serializeFileHeader(FileHeader header) {
    PageGuard pageGuard;
    ASSIGN_OR_RETURN(pageGuard, fetchPage(0));
    ByteSpan headerSpan = pageGuard.subspan(0, sizeof(FileHeader));

    // serialize header
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

    return Ok();
}

DbResult<void> LRUPager::flush(PageNum pageNum) {
    FrameIndex frameIndex = *pageMap_[pageNum];
    Frame& frame = frames_[frameIndex];

    if (frame.dirty) {
        RETURN_ON_ERROR(pageIO_->writePage(pageNum, frame.data));
        frame.dirty = false;
    }

    return Ok();
}

} // namespace LiliumDB