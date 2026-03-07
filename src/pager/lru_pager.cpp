#include "lru_pager.h"

#include "storage/std_page_io.h"

#include <cstddef>
#include <cstring>
#include <ctime>

namespace LiliumDB {

Result<std::unique_ptr<Pager>>
LRUPager::open(std::string_view path, OpenMode mode, size_t poolSize) {
    TRY_RESULT_BIND(StdPageIO::open(path, mode), p);

    LRUPager* lruPager = new LRUPager(std::move(p), poolSize);

    if (lruPager->pageIO_->pageCount() == 0) { // new uninitialized file
        lruPager->initFile();
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
    FileFlags flags = header.read<FileFlags>(offsetof(FileHeader, fileFlags));

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
    allocateFrame(0);

    // write file header to page zero
    auto s = serializeFileHeader(header);

    if (!s.isOk()) {
        return s;
    }
    return Status::ok();
}

LRUPager::FrameIndex LRUPager::allocateFrame(PageNum pageNum) {
    FrameIndex idx = nextFreeFrame_ < frames_.size() ? nextFreeFrame_++ : evictLastUsedPage();
    Frame frame{ByteSpan(&pool_.data()[idx * PAGE_SIZE], PAGE_SIZE), pageNum, 0, false};

    frames_[idx] = frame;
    auto iter = lruList_.emplace(lruList_.begin(), idx);
    pageMap_.insert({pageNum, iter});

    return idx;
}

Status LRUPager::serializeFileHeader(FileHeader header) {
    TRY_STATUS_BIND(fetchPage(0), pg);

    // serialize header
    ByteSpan span = pg.span();

    span.write(0, header.magicBytes, sizeof(header.magicBytes));
    SPAN_WRITE_FIELD(span, header, fileFlags);
    SPAN_WRITE_FIELD(span, header, versionMajor);
    SPAN_WRITE_FIELD(span, header, versionMinor);
    SPAN_WRITE_FIELD(span, header, pageCount);
    SPAN_WRITE_FIELD(span, header, fileCreated);
    SPAN_WRITE_FIELD(span, header, lastModified);
    SPAN_WRITE_FIELD(span, header, freespaceHead);
    // skip reserved bytes
    SPAN_WRITE_FIELD(span, header, checksum);

    return Status::ok();
}

} // namespace LiliumDB