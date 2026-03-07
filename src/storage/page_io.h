#ifndef LILIUMDB_PAGE_IO_H
#define LILIUMDB_PAGE_IO_H

#include "common/core.h"
#include "utils/byte_span.h"

namespace LiliumDB {

enum class OpenMode { ReadWrite, ReadOnly };

class PageIO {
public:
    virtual ~PageIO() = default;

    virtual VoidResult  close() = 0;
    virtual VoidResult  readPage(PageNum page, ByteSpan dst) = 0;
    virtual VoidResult  writePage(PageNum page, ByteView src) = 0;
    virtual bool        isOpen() const = 0;
    virtual uint32_t    pageCount() const = 0;
};

} // namespace LiliumDB

#endif