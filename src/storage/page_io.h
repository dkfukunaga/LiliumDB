#ifndef LILIUMDB_PAGE_IO_H
#define LILIUMDB_PAGE_IO_H

#include "common/types.h"
#include "common/status.h"
#include "common/byte_span.h"

namespace LiliumDB {

enum class OpenMode { ReadWrite, ReadOnly };

class PageIO {
public:
    virtual ~PageIO() = default;

    virtual Status  close() = 0;
    virtual Status  readPage(PageNum page, ByteSpan dst) = 0;
    virtual Status  writePage(PageNum page, ByteView src) = 0;
    virtual bool    isOpen() const = 0;
};

} // namespace LiliumDB

#endif