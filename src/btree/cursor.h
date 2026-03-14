#ifndef LILIUMDB_CURSOR_H
#define LILIUMDB_CURSOR_H

#include "common/core.h"
#include "pager/page_guard.h"
#include "utils/byte_span.h"

namespace LiliumDB {

class Cursor {
public:
    Cursor();
    ~Cursor();

    ByteSpan        key() const;
    ByteSpan        value() const;
    bool            valid() const { return valid_; }

    DbResult<void>  next();
    DbResult<void>  prev();
private:
    Pager*      pager_;
    PageGuard   page_;
    SlotIndex   slot_;
    bool        valid_;
};

} // namespace LiliumDB

#endif