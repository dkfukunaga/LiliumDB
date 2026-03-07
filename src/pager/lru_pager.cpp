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
    }
    
    RETURN_ON_ERROR(lruPager->validateFileHeader());

    std::unique_ptr<Pager> pager = std::move(lruPager);
    return Ok(std::move(pager));
}

VoidResult LRUPager::validateFileHeader() {
    PageGuard pageGuard;
    ASSIGN_OR_RETURN(pageGuard, fetchPage(0));
    ByteView header = pageGuard.view().subview(0, sizeof(FileHeader));

    // check magic bytes
    constexpr size_t mBytesLen = sizeof(FileHeader::magicBytes);
    uint8_t mBytes[mBytesLen];
    header.read(offsetof(FileHeader, magicBytes), mBytes, mBytesLen);
    if (memcmp(mBytes, MAGIC_BYTES.data(), mBytesLen) != 0) {
        return Err(Status::fileInvalid("Incorrect file type."));
    }

    // check for corrupt flag
    FileFlags flags = header.get<FileFlags>(offsetof(FileHeader, fileFlags));
    if (flags.has(FileFlag::Corrupt)) {
        return Err(Status::corrupt("File is corrupt."));
    }

    // check major version
    uint8_t vMajor = header.get<uint8_t>(offsetof(FileHeader, versionMajor));
    if (vMajor != VERSION_MAJOR) {
        return Err(Status::fileInvalid("Incompatible file version."));
    }

    // read pageCount
    appendStart_ = header.get<uint32_t>(offsetof(FileHeader, pageCount));
    return Ok(Success);
}

VoidResult LRUPager::initFile() {
    // initialize new file header
    FileHeader header;

    auto now = static_cast<int64_t>(std::time(nullptr));

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
    return serializeFileHeader(header);
}

DbResult<LRUPager::FrameIndex> LRUPager::allocatePage(PageNum pageNum) {
    FrameIndex index;
    if (nextFreeFrame_ < frames_.size()) {
        index = nextFreeFrame_++;
    } else {
        FrameIndex temp;
        ASSIGN_OR_RETURN(temp, evictLastUsedPage());
        index = temp;
    }
    Frame frame(pool_, index, pageNum);

    frames_[index] = frame;
    auto iter = lruList_.emplace(lruList_.begin(), index);
    pageMap_.insert({pageNum, iter});

    return Ok(index);
}

VoidResult LRUPager::serializeFileHeader(FileHeader header) {
    PageGuard pageGuard;
    ASSIGN_OR_RETURN(pageGuard, fetchPage(0));

    // serialize header
    pageGuard.span().write(0, header.magicBytes, sizeof(header.magicBytes));
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, fileFlags);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, versionMajor);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, versionMinor);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, pageCount);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, fileCreated);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, lastModified);
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, freespaceHead);
    // skip reserved bytes
    SPAN_WRITE_STRUCT_FIELD(pageGuard.span(), header, checksum);

    return Ok(Success);
}

} // namespace LiliumDB