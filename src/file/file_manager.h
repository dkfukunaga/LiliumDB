#ifndef LILIUMDB_FILE_MANAGER_H
#define LILIUMDB_FILE_MANAGER_H

#include "common/types.h"
#include "common/byte_span.h"

namespace LiliumDB {

enum class OpenMode { ReadWrite, ReadOnly };

class FileManager {
public:
    virtual ~FileManager() = default;
    virtual Status open(std::string_view path, OpenMode mode = OpenMode::ReadWrite) = 0;
    virtual Status close() = 0;
    virtual Status readPage(PageNum page, ByteSpan dst) = 0;
    virtual Status writePage(PageNum page, ByteView src) = 0;
    virtual bool   isOpen() const = 0;
};

} // namespace LiliumDB

#endif