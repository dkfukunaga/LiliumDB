#include "lru_pager.h"

#include "storage/std_page_io.h"

#include <cstddef>
#include <cstring>
#include <ctime>

namespace LiliumDB {

Result<std::unique_ptr<Pager>>
LRUPager::open(std::string_view path, OpenMode mode, size_t poolSize) {
    TRY_RESULT_BIND(StdPageIO::open(path, mode), pio);

    LRUPager* lruPager = new LRUPager(std::move(pio), poolSize);

    if (lruPager->pageIO_->pageCount() == 0) { // new uninitialized file
        TRY_RESULT_WRAP(lruPager->initFile());
    }

    std::unique_ptr<Pager> pager(lruPager);
    return Ok(std::move(pager));
}

bool LRUPager::validateFileHeader(ByteView header) const {
    // read magic bytes
    constexpr size_t mBytesLen = sizeof(FileHeader::magicBytes);
    uint8_t mBytes[mBytesLen];
    header.read(offsetof(FileHeader, magicBytes), mBytes, mBytesLen);

    // read flags
    FileFlags flags = header.get<FileFlags>(offsetof(FileHeader, fileFlags));

    // read version
    uint8_t vMajor, vMinor;
    header.read(offsetof(FileHeader, versionMajor), &vMajor, 1);
    header.read(offsetof(FileHeader, versionMinor), &vMinor, 1);

    return memcmp(mBytes, MAGIC_BYTES.data(), mBytesLen) == 0
           && !flags.has(FileFlag::Corrupt)
           && vMajor == VERSION_MAJOR
           && vMinor == VERSION_MINOR;
}

Status LRUPager::initFile() {
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
    TRY_STATUS(allocatePage(0));

    // write file header to page zero
    return serializeFileHeader(header);
}

Result<LRUPager::FrameIndex> LRUPager::allocatePage(PageNum pageNum) {
    FrameIndex index;
    if (nextFreeFrame_ < frames_.size()) {
        index = nextFreeFrame_++;
    } else {
        TRY_RESULT_BIND(evictLastUsedPage(), temp);
        index = temp;
    }
    Frame frame(pool_, index, pageNum);

    frames_[index] = frame;
    auto iter = lruList_.emplace(lruList_.begin(), index);
    pageMap_.insert({pageNum, iter});

    return Ok(index);
}

Status LRUPager::serializeFileHeader(FileHeader header) {
    TRY_STATUS_BIND(fetchPage(0), pageGuard);

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

    return Status::ok();
}

} // namespace LiliumDB