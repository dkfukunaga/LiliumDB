#ifndef LILIIUMDB_BTREE_H
#define LILIIUMDB_BTREE_H

#include "common/core.h"
#include "utils/byte_span.h"
#include "pager/pager.h"
#include "cursor.h"

#include <vector>
#include <optional>

namespace LiliumDB {

class BTree {
public:
    BTree(Pager& pager, PageNum root, PageType type)
        : pager_(pager)
        , root_(root)
        , type_(type) { }

    DbResult<void>          insert(ByteView key, ByteView value);
    DbResult<bool>          remove(ByteView key);
    DbResult<Cursor>        seek(ByteView key);
    DbResult<Cursor>        first();
    DbResult<Cursor>        last();

private:
    struct PagePosition {
        PageGuard   page;
        SlotIndex   index;
    };
    using ParentStack = std::vector<PagePosition>;

    struct SplitResult {
        std::vector<uint8_t>    key;
        PageNum                 leftChild;
        PageNum                 rightChild;
    };
    // using MaybeSplit = std::optional<SplitResult>;

    using MergeResult = std::optional<SlotIndex>;

    enum Direction {fromLeft, fromRight};

    Pager& pager_;
    PageNum root_;
    PageType type_;

    DbResult<ParentStack> search(ByteView key) const;
    SlotIndex searchInner(PageGuard& page, ByteView key) const;
    SlotIndex searchLeaf(PageGuard& page, ByteView key) const;
    uint16_t compactPage(PageGuard& page);
    void deleteEntry(PageGuard& page, SlotIndex index);

    void insertIntoLeaf(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        ByteView value);
    void insertIntoInner(
        PageGuard& page,
        SlotIndex index,
        ByteView key,
        PageNum leftChild,
        PageNum rightChild);

    DbResult<void> splitAndInsert(
        ParentStack&& stack,
        ByteView key,
        ByteView value);
    DbResult<SplitResult> splitLeaf(
        PageGuard& leftPage,
        SlotIndex index,
        ByteView key,
        ByteView value);
    DbResult<SplitResult> splitInner(
        PageGuard& leftPage,
        SlotIndex index,
        std::vector<uint8_t> key,
        PageNum leftChild,
        PageNum rightChild);
    DbResult<void> promoteRoot(
        PageGuard& rootPage,
        PageGuard& rightPage,
        ByteView key
    );

    DbResult<void> rebalance(ParentStack&& stack);
    void mergePages(
        PageGuard& leftPage,
        PageGuard& rightPage,
        bool isLeaf
    );
    bool takeFromSibling(
        PageGuard& page,
        PageGuard& sibling, 
        PageGuard& parent,
        SlotIndex separatorIndex,
        Direction direction
    );
    bool takeFromLeft(
        PageGuard& page,
        PageGuard& leftSibling,
        PageGuard& parent,
        SlotIndex index
    );
    bool takeFromRight(
        PageGuard& page,
        PageGuard& rightSibling, 
        PageGuard& parent,
        SlotIndex index
    );
    bool rotateFromSibling(
        PageGuard& page,
        PageGuard& sibling, 
        PageGuard& parent,
        SlotIndex separatorIndex,
        Direction direction
    );
    DbResult<std::optional<PageGuard>> getLeftSibling(
        PageGuard& page,
        PageGuard& parent,
        SlotIndex index
    );
    DbResult<std::optional<PageGuard>> getRightSibling(
        PageGuard& page,
        PageGuard& parent,
        SlotIndex index
    );
};

} // namespace LiliumDB

#endif