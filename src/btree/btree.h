#ifndef LILIIUMDB_BTREE_H
#define LILIIUMDB_BTREE_H

#include "common/core.h"
#include "utils/byte_span.h"
#include "pager/pager.h"
#include "cursor.h"

#include <vector>

namespace LiliumDB {

class BTree {
public:
    BTree(Pager& pager, PageNum root): pager_(pager), root_(root) { }

    DbResult<void>          insert(ByteView key, ByteView value);
    DbResult<void>          remove(ByteView key);
    DbResult<Cursor>        seek(ByteView key);
    DbResult<Cursor>        first();
    DbResult<Cursor>        last();
private:
    struct PagePosition {
        PageGuard   page;
        SlotIndex   slot;
    };
    using ParentStack = std::vector<PagePosition>;

    Pager&  pager_;
    PageNum root_;

    DbResult<ParentStack> leafSearch(ByteView key) const;
    void insertIntoLeaf(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        ByteView value);
    void insertIntoInternal(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        PageNum child);
    DbResult<void> splitAndInsert(
        PageGuard page,
        SlotIndex index,
        ParentStack&& stack,
        ByteView key,
        ByteView value);

};

} // namespace LiliumDB

#endif