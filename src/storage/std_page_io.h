#ifndef LILIUMDB_STD_PAGE_IO_H
#define LILIUMDB_STD_PAGE_IO_H

#include <fstream>

#include "page_io.h"

namespace LiliumDB {

class StdPageIO : public PageIO {
public:
    StdPageIO(): pageCount_(0) { }
    ~StdPageIO() override;

    Status          open(std::string_view path, OpenMode mode) override;
    Status          close() override;
    Status          readPage(PageNum page, ByteSpan dst) override;
    Status          writePage(PageNum page, ByteView src) override;

    bool            isOpen() const override;
private:
    std::fstream    file_;
    OpenMode        mode_;
    PageNum         pageCount_;
};


} // namespace LiliumDB

#endif