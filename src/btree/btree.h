#ifndef LILIIUMDB_BTREE_H
#define LILIIUMDB_BTREE_H

#include "common/core.h"
#include "utils/byte_span.h"

namespace LiliumDB {

class BTree {
public:


    DbResult<void>      insert(ByteView key, ByteView data);
    DbResult<ByteSpan>  find(ByteView key);
    DbResult<void>      remove(ByteView key);
private:
};

} // namespace LiliumDB

#endif