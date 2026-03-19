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
    uint16_t compact(PageGuard& page);

    void insertIntoLeaf(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        ByteView value);
    void insertIntoInner(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        PageNum child);

    DbResult<void> splitAndInsert(
        PageGuard page,
        ParentStack&& stack,
        SlotIndex index,
        ByteView key,
        ByteView value);
    DbResult<std::vector<uint8_t>> splitLeaf(
        PageGuard page,
        SlotIndex index,
        ByteView key,
        ByteView value);
    DbResult<std::vector<uint8_t>> splitInner(
        PageGuard page,
        SlotIndex index,
        ByteView key,
        PageNum child);

    DbResult<void> mergeOrRedistribute(PageGuard page, ParentStack&& stack);
    DbResult<std::vector<uint8_t>> mergeLeaf(PageGuard page);
    DbResult<std::vector<uint8_t>> mergeInner(PageGuard page);

};

} // namespace LiliumDB

#endif