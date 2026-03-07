#ifndef LILIUMDB_STD_PAGE_IO_H
#define LILIUMDB_STD_PAGE_IO_H

#include "page_io.h"

#include "common/core.h"
#include "common/byte_span.h"

#include <fstream>
#include <memory>

namespace LiliumDB {

class StdPageIO : public PageIO {
public:
    static DbResult<std::unique_ptr<StdPageIO>> open(std::string_view path, OpenMode mode);
    ~StdPageIO() override { if (isOpen()) close(); }

    VoidResult          close() override;
    VoidResult          readPage(PageNum page, ByteSpan dst) override;
    VoidResult          writePage(PageNum page, ByteView src) override;

    bool                isOpen() const override;
    uint32_t            pageCount() const override { return pageCount_; }

private:
    StdPageIO(OpenMode mode): mode_(mode), pageCount_(0) { }

    std::fstream        file_;
    OpenMode            mode_;
    PageNum             pageCount_;
};


} // namespace LiliumDB

#endif