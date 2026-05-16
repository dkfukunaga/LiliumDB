#include "btree.h"

#include <algorithm>

#include "btree_page.h"

using namespace LiliumDB::BTreePage;

namespace LiliumDB {

DbResult<void> BTree::insert(ByteView key, ByteView value) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, search(key));

    PageGuard& page = stack.back().page;
    SlotIndex index = stack.back().index;
    PageHeader header = page.getHeader();

    uint16_t freeSpace = static_cast<uint16_t>(
        (PAGE_END_OFFSET - header.slotCount * sizeof(Slot)) - header.freeOffset);
    uint16_t insertSize = static_cast<uint16_t>(
        key.size() + value.size() + sizeof(Slot) + sizeof(KeyValueHeader));

    if (insertSize > freeSpace) {
        freeSpace = compactPage(page);
    }

    if (insertSize <= freeSpace) {
        insertIntoLeaf(page, index, key, value);
    } else {
        RETURN_ON_ERROR(splitAndInsert(std::move(stack), key, value));
    }

    // printf("complete!\n");

    return Ok();
}

DbResult<bool> BTree::remove(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, search(key));

    // get a reference to the leaf page on top of the stack
    PageGuard& page = stack.back().page;
    // get leaf page's index in its parent page
    SlotIndex index = stack.back().index;

    // check for empty tree
    if (index == INVALID_SLOT) {
        return Ok(false);
    }

    auto pageKey = getKey(page, index);

    // return false if exact key not found
    if (key.size() != pageKey.size() ||
        memcmp(key.data(), pageKey.data(), key.size()) != 0) {
        return Ok(false);
    }

    // remove slot
    deleteEntry(page, index);

    // call rebalance if page is now under minimum occupancy
    if (usedSpace(page) < page.minOccupancy()) {
        RETURN_ON_ERROR(rebalance(std::move(stack)));
    }

    return Ok(true);
}

DbResult<Cursor> BTree::seek(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, search(key));
    PageGuard page = std::move(stack.back().page);
    SlotIndex index = stack.back().index;
    stack.pop_back();

    if (index == INVALID_SLOT)
        return Ok(Cursor());

    return Ok(Cursor(&pager_, std::move(page), index));
}

DbResult<Cursor> BTree::first() {
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(root_));
    auto header = page.getHeader();

    if (header.slotCount == 0) {
        return Ok(Cursor()); // invalid cursor
    }

    while (header.level > 0) {
        ASSIGN_OR_RETURN(page, pager_.fetchPage(getChild(page, 0)));
        header = page.getHeader();
    }

    return Ok(Cursor(&pager_, std::move(page), 0));
}

DbResult<Cursor> BTree::last() {
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(root_));
    auto header = page.getHeader();

    if (header.slotCount == 0) {
        return Ok(Cursor()); // invalid cursor
    }

    while (header.level > 0) {
        ASSIGN_OR_RETURN(page, pager_.fetchPage(header.next));
        header = page.getHeader();
    }

    return Ok(Cursor(&pager_, std::move(page), header.slotCount - 1));
}

DbResult<BTree::ParentStack> BTree::search(ByteView key) const {
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(root_));
    PageHeader header = page.getHeader();
    ParentStack stack;
    SlotIndex index = INVALID_SLOT;

    if (header.level == INVALID_TREE_LEVEL) {
        header.level = 0;
        page.setHeader(header);
        stack.push_back({std::move(page), 0});
        return Ok(std::move(stack));
    }

    while (header.level > 0) {
        index = searchInner(page, key);
        PageNum child = getChild(page, index);

        stack.push_back({std::move(page), index});
        ASSIGN_OR_RETURN(page, pager_.fetchPage(child));
        header = page.getHeader();
    }
    index = searchLeaf(page, key);
    stack.push_back({std::move(page), index});
    return Ok(std::move(stack));
}

SlotIndex BTree::searchInner(PageGuard& page, ByteView key) const {
    auto header = page.getHeader();

    // for rare cases of a root page with a single child
    if (header.slotCount == 0) {
        return 0;
    }

    // binary search of keys
    SlotIndex mid;
    SlotIndex low = 0;
    SlotIndex high = header.slotCount - 1;

    while (low <= high) {
        mid = static_cast<SlotIndex>(low + (high - low) / 2);
        auto slot = page.view().get<Slot>(slotOffset(mid));
        auto keyHeader = page.view().get<KeyHeader>(slot.offset);
        auto slotKey = page.subview(slot.offset + sizeof(KeyHeader), keyHeader.keySize);

        auto comp = memcmp(key.data(), slotKey.data(), std::min(key.size(), slotKey.size()));
        if (comp == 0) {
            comp = (int) key.size() - (int) slotKey.size();
        }
        if (comp == 0) {
            return mid;
        } else if (comp < 0) {
            if (mid == 0) return mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    return low;
}

SlotIndex BTree::searchLeaf(PageGuard& page, ByteView key) const {
    SlotIndex low = 0;
    SlotIndex high = page.getHeader().slotCount - 1;

    while (low <= high) {
        SlotIndex mid = static_cast<SlotIndex>(low + (high - low) / 2);
        auto slot = page.view().get<Slot>(slotOffset(mid));
        auto keyValueHeader = page.view().get<KeyValueHeader>(slot.offset);
        auto midKey = page.subview(slot.offset + sizeof(KeyValueHeader), keyValueHeader.keySize);

        auto comp = memcmp(key.data(), midKey.data(), std::min(key.size(), midKey.size()));
        if (comp == 0) {
            comp = (int) key.size() - (int) midKey.size();
        }
        if (comp == 0) {
            return mid;
        } else if (comp < 0) {
            if (mid == 0) return mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    return low;
}

uint16_t BTree::compactPage(PageGuard& page) {
    auto header = page.getHeader();
    auto start = page.usableStart();

    // create a buffer to write compacted cells to
    size_t cellBufSize = slotOffset(header.slotCount) - start;
    std::vector<uint8_t> cellBuf(cellBufSize);
    ByteSpan cellBuffSpan(cellBuf);

    // create a buffer to write update slots to
    size_t slotBufSize = header.slotCount * sizeof(Slot);
    std::vector<uint8_t> slotBuf(slotBufSize);
    ByteSpan slotBufSpan(slotBuf);

    // iterate through slots and write cells sequentially to the buffer
    uint16_t cellBufOffset = 0;
    assert(slotBufSize >= sizeof(Slot));
    uint16_t slotBufOffset = static_cast<uint16_t>(slotBufSize - sizeof(Slot));
    for (int i = 0; i < header.slotCount; ++i) {
        // write cell to cell buffer
        Slot slot = page.view().get<Slot>(slotOffset(i));
        cellBuffSpan.write(cellBufOffset, page.subview(slot.offset, slot.size));

        // update slot and write to slot buffer
        slot.offset = start + cellBufOffset;
        slotBufSpan.put<Slot>(slotBufOffset - static_cast<uint16_t>(i) * sizeof(Slot), slot);

        cellBufOffset += slot.size;
        // assert(slotBufOffset >= sizeof(Slot));
        // slotBufOffset = static_cast<uint16_t>(slotBufOffset - sizeof(Slot));
    }

    // write compacted cell buffer and slot buffer to page
    page.span().write(start, cellBuffSpan);
    page.span().write(slotOffset(header.slotCount - 1), slotBufSpan);

    // update page header
    header.freeOffset = start + cellBufOffset;
    page.setHeader(header);

    // return total free space
    return slotOffset(header.slotCount - 1) - header.freeOffset;
}

void BTree::deleteEntry(PageGuard& page, SlotIndex index) {
    auto header = page.getHeader();

    // update header.next for inner pages if index is last slot
    if (header.level > 0 && index == header.slotCount - 1) {
        header.next = getChild(page, index);
    }

    // remove slot and adjust over slots in the slot table
    for (SlotIndex i = index + 1; i < header.slotCount; ++i) {
        Slot slot = page.view().get<Slot>(slotOffset(i));
        page.span().put<Slot>(slotOffset(i - 1), slot);
    }

    // decrement slot count
    header.slotCount--;
    page.setHeader(header);
}

void BTree::insertIntoLeaf(PageGuard& page, SlotIndex index, ByteView key, ByteView value) {
    auto header = page.getHeader();
    auto freeStart = header.freeOffset;
    uint16_t totalSize = static_cast<uint16_t>(sizeof(KeyValueHeader) + key.size() + value.size());

    // write key-value cell with header
    KeyValueHeader kvHeader{static_cast<uint16_t>(key.size()), static_cast<uint16_t>(value.size())};
    page.span().put<KeyValueHeader>(freeStart, kvHeader);
    page.span().write(freeStart + sizeof(KeyValueHeader), key);
    page.span().write(freeStart + sizeof(KeyValueHeader) + key.size(), value);

    // relocate slots to make space for new slot if there is at least 1 slot already
    if (header.slotCount > 0) {
        for (int i = header.slotCount - 1; i >= static_cast<int>(index); --i) {
            page.span().copy_within(slotOffset(i + 1), slotOffset(i), sizeof(Slot));
        }
    }
    page.span().put<Slot>(slotOffset(index), Slot{freeStart, totalSize});

    // update page header
    header.slotCount++;
    header.freeOffset += totalSize;
    page.setHeader(header);
}

void BTree::insertIntoInner(
    PageGuard& page,
    SlotIndex index,
    ByteView key,
    PageNum leftChild,
    PageNum rightChild
) {
    auto header = page.getHeader();
    auto cellOffset = header.freeOffset;
    uint16_t totalSize = static_cast<uint16_t>(sizeof(KeyHeader) + key.size());

    // write key-value cell with header
    KeyHeader keyHeader{static_cast<uint16_t>(key.size()), leftChild};
    page.span().put<KeyHeader>(cellOffset, keyHeader);
    page.span().write(cellOffset + sizeof(KeyHeader), key);

    // relocate slots to make space for new slot
    for (int i = header.slotCount - 1; i >= static_cast<int>(index); --i) {
        page.span().copy_within(slotOffset(i + 1), slotOffset(i), sizeof(Slot));
    }
    page.span().put<Slot>(slotOffset(index), Slot{cellOffset, totalSize});

    // update page header
    header.slotCount++;
    header.freeOffset += totalSize;
    page.setHeader(header);

    // update next pointer to point to right page
    if (rightChild != INVALID_PAGE) {
        setChild(page, index + 1, rightChild);
    }
}

DbResult<void> BTree::splitAndInsert(
    ParentStack&& stack,
    ByteView key,
    ByteView value
) {
    // pop leaf page from parent stack
    PageGuard page = std::move(stack.back().page);
    SlotIndex index = stack.back().index;
    stack.pop_back();

    // split and insert into leaf page
    BTree::SplitResult splitResult;
    ASSIGN_OR_RETURN(splitResult, splitLeaf(page, index, key, value));

    bool needsRootPromotion = true;
    while (!stack.empty()) {
        // pop inner page from parent stack
        page = std::move(stack.back().page);
        stack.pop_back();
        PageHeader header = page.getHeader();

        // calculate free space and insertion size
        uint16_t freeSpace = static_cast<uint16_t>(
            (PAGE_END_OFFSET - header.slotCount * sizeof(Slot)) - header.freeOffset);
        uint16_t insertSize = static_cast<uint16_t>(
            splitResult.key.size() + sizeof(PageNum) + sizeof(Slot) + sizeof(KeyHeader));

        // find index for separator key
        index = searchInner(page, ByteView(splitResult.key));

        // if the separator key fits, insert and exit loop
        // else, split and continue root
        if (freeSpace >= insertSize) {
            insertIntoInner(page, index, ByteView(splitResult.key), splitResult.leftChild, splitResult.rightChild);
            needsRootPromotion = false;
            break;
        } else if (!stack.empty()) {
            ASSIGN_OR_RETURN(splitResult, splitInner(page, index, splitResult.key, splitResult.leftChild, splitResult.rightChild));
        }
    }

    // if the stack is empty, we've reached the root and need to promote the left page to a new root page
    if (needsRootPromotion && stack.empty()) {
        if (page.getHeader().level > 0) {
            ASSIGN_OR_RETURN(splitResult, splitInner(page, index, splitResult.key, splitResult.leftChild, splitResult.rightChild));
        }
        PageGuard rightPage;
        ASSIGN_OR_RETURN(rightPage, pager_.fetchPage(splitResult.rightChild));
        RETURN_ON_ERROR(promoteRoot(page, rightPage, ByteView(splitResult.key)));
    }

    return Ok();
}

DbResult<BTree::SplitResult> BTree::splitLeaf(
    PageGuard& leftPage,
    SlotIndex index,
    ByteView key,
    ByteView value
) {
    // helper lambdas

    // returns total size of an entry and its slot
    auto logicalSize = [&](SlotIndex logical) -> uint16_t {
        if (logical == index)
            return static_cast<uint16_t>(KEY_VALUE_SLOT_OVERHEAD + key.size() + value.size());
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        return sizeof(Slot) + s.size;
    };

    struct Entry { std::vector<uint8_t> key; std::vector<uint8_t> value; };

    // returns vectors of an entry's key and value, slotting the new entry
    // into its logical place in the sequence of slots
    auto logicalEntry = [&](SlotIndex logical) -> Entry {
        if (logical == index)
            return { key.toVector(), value.toVector() };
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        KeyValueHeader kv = leftPage.view().get<KeyValueHeader>(s.offset);
        return {
            leftPage.view().readAsVector(s.offset + KEY_VALUE_HEADER_SIZE, kv.keySize),
            leftPage.view().readAsVector(static_cast<uint16_t>(s.offset + KEY_VALUE_HEADER_SIZE + kv.keySize), kv.valueSize)
        };
    };

    auto leftHeader = leftPage.getHeader();
    uint16_t slotCount = leftHeader.slotCount;
    assert(slotCount > 0);
    uint16_t minOccupancy = leftPage.minOccupancy();

    SlotIndex splitIndex = 0;
    uint16_t tempPageSize = 0;
    uint16_t leftPageSize = 0;
    uint16_t prevDiff = 0;
    uint16_t nextDiff = 0;

    while (tempPageSize < minOccupancy) {
        assert(splitIndex <= slotCount); // slotCount + 1 logical entries total
        tempPageSize += logicalSize(splitIndex);
        
        if (tempPageSize >= minOccupancy) {
            prevDiff = minOccupancy - leftPageSize;
            nextDiff = tempPageSize - minOccupancy;
            if (nextDiff <= prevDiff)
                splitIndex++;
            break;
        } else {
            leftPageSize = tempPageSize;
            splitIndex++;
        }
    }

    std::vector<uint8_t> separatorKey = logicalEntry(splitIndex).key;

    // allocate a new page for rightPage
    PageGuard rightPage;
    ASSIGN_OR_RETURN(rightPage, pager_.newPage(type_));

    // update header for rightPage
    auto rightHeader = rightPage.getHeader();
    rightHeader.level = 0;
    rightHeader.next = leftHeader.next;
    rightHeader.prev = leftPage.pageNum();
    // slotCount and freeOffset updated by insertIntoLeaf()
    rightPage.setHeader(rightHeader);

    // insert entries from the split entry onwards into rightPage
    for (auto i = splitIndex; i < slotCount + 1; ++i) {
        Entry entry = logicalEntry(i);
        insertIntoLeaf(rightPage, i - splitIndex, ByteView(entry.key), ByteView(entry.value));
    }
    
    // if leftPage.next pointed to a valid next page, update nextpage.prev to point to rightPage
    if (leftHeader.next != INVALID_PAGE) {
        PageGuard nextPage;
        ASSIGN_OR_RETURN(nextPage, pager_.fetchPage(leftHeader.next));
        auto nextHeader = nextPage.getHeader();
        nextHeader.prev = rightPage.pageNum();
        nextPage.setHeader(nextHeader);
    }

    // update header for leftPage and compact entries
    leftHeader.next = rightPage.pageNum();
    leftHeader.slotCount = index < splitIndex ? splitIndex - 1 : splitIndex;
    leftPage.setHeader(leftHeader);
    compactPage(leftPage);
    if (index < splitIndex) {
        insertIntoLeaf(leftPage, index, ByteView(key), ByteView(value));
    }

    return Ok(SplitResult{separatorKey, leftPage.pageNum(), rightPage.pageNum()});
}

DbResult<BTree::SplitResult> BTree::splitInner(
    PageGuard& leftPage,
    SlotIndex index,
    std::vector<uint8_t> key,
    PageNum leftChild,
    PageNum rightChild
) {
    // helper lambdas

    // returns total size of an entry and its slot
    auto logicalSize = [&](SlotIndex logical) -> uint16_t {
        if (logical == index)
            return KEY_SLOT_OVERHEAD + static_cast<uint16_t>(key.size());
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        return sizeof(Slot) + s.size;
    };

    struct Entry { std::vector<uint8_t> key; PageNum child; };

    // returns vectors of an entry's key and child page, slotting the new entry
    // into its logical place in the sequence of slots
    auto logicalEntry = [&](SlotIndex logical) -> Entry {
        if (logical == index)
            return { key, leftChild };
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        KeyHeader kh = leftPage.view().get<KeyHeader>(s.offset);
        return {
            leftPage.view().readAsVector(s.offset + KEY_HEADER_SIZE, kh.keySize),
            logical == index + 1 ? rightChild : kh.childPage
        };
    };

    auto leftHeader = leftPage.getHeader();
    uint16_t slotCount = leftHeader.slotCount;
    assert(slotCount > 0);
    uint16_t minOccupancy = leftPage.minOccupancy();

    SlotIndex splitIndex = 0;
    uint16_t tempPageSize = 0;
    uint16_t leftPageSize = 0;
    uint16_t prevDiff = 0;
    uint16_t nextDiff = 0;

    while (tempPageSize < minOccupancy) {
        assert(splitIndex < slotCount + 1); // slotCount + 1 logical entries total
        tempPageSize += logicalSize(splitIndex);
        
        if (tempPageSize >= minOccupancy) {
            prevDiff = minOccupancy - leftPageSize;
            nextDiff = tempPageSize - minOccupancy;
            if (nextDiff <= prevDiff)
                splitIndex++;
            break;
        } else {
            leftPageSize = tempPageSize;
            splitIndex++;
        }
    }

    std::vector<uint8_t> separatorKey = logicalEntry(splitIndex).key;
    auto leftNext = logicalEntry(splitIndex).child;

    // allocate a new page for rightPage
    PageGuard rightPage;
    ASSIGN_OR_RETURN(rightPage, pager_.newPage(type_));
    setChild(leftPage, splitIndex, leftChild);

    // update header for rightPage
    auto rightHeader = rightPage.getHeader();
    rightHeader.level = leftHeader.level;
    rightHeader.next = leftHeader.next;
    // slotCount and freeOffset updated by insertIntoInner()
    rightPage.setHeader(rightHeader);

    // insert entries from the split entry onwards into rightPage
    for (SlotIndex i = splitIndex + 1; i < slotCount + 1; ++i) {
        Entry entry = logicalEntry(i);
        PageNum nextPage;
        if (i < slotCount) {
            nextPage = logicalEntry(i+1).child;
        } else {
            nextPage = leftHeader.next;
        }
        insertIntoInner(rightPage,
            static_cast<SlotIndex>(i - splitIndex - 1),
            ByteView(entry.key),
            entry.child, nextPage);
    }

    // update header for leftPage and compact entries
    leftHeader.slotCount = index < splitIndex ? splitIndex - 1 : splitIndex;
    leftHeader.next = leftNext;
    leftPage.setHeader(leftHeader);
    compactPage(leftPage);
    if (index < splitIndex) {
        insertIntoInner(leftPage, index, ByteView(key), leftChild, rightChild);
    }

    return Ok(SplitResult{separatorKey, leftPage.pageNum(), rightPage.pageNum()});
}

DbResult<void> BTree::promoteRoot(
    PageGuard& rootPage,
    PageGuard& rightPage,
    ByteView key
) {
    auto rootHeader = rootPage.getHeader();
    // allocate a new left page
    PageGuard leftPage;
    ASSIGN_OR_RETURN(leftPage, pager_.newPage(type_));

    // copy old left page into new left page
    if (rootHeader.level == 0) {
        for (SlotIndex i = 0; i < rootHeader.slotCount; ++i) {
            auto k = getKey(rootPage, i);
            auto v = getValue(rootPage, i);
            insertIntoLeaf(leftPage, i, k, v);
        }
    } else {
        for (SlotIndex i = 0; i < rootHeader.slotCount; ++i) {
            auto k = getKey(rootPage, i);
            auto c = getChild(rootPage, i);
            PageNum n;
            if (i == rootHeader.slotCount - 1) {
                n = rootHeader.next;
            } else {
                n = getChild(rootPage, i + 1);
            }
            insertIntoInner(leftPage, i, k, c, n);
        }
    }

    // update headers
    auto leftHeader = leftPage.getHeader();
    auto rightHeader = rightPage.getHeader();
    
    leftHeader.level = rootHeader.level;
    leftHeader.next = rootHeader.next;
    leftPage.setHeader(leftHeader);

    rightHeader.level = rootHeader.level;
    if (rightHeader.level == 0) {
        rightHeader.prev = leftPage.pageNum();
    }
    rightPage.setHeader(rightHeader);

    rootHeader.level++;
    rootHeader.slotCount = 0;
    rootHeader.freeOffset = rootPage.usableStart();
    rootHeader.next = rightPage.pageNum();
    rootHeader.prev = INVALID_PAGE;
    // rootHeader.next is already set to rightPage.pageNum
    rootPage.setHeader(rootHeader);

    // insert separator key
    insertIntoInner(rootPage, 0, key, leftPage.pageNum(), rightPage.pageNum());

    return Ok();
}

DbResult<void> BTree::rebalance(ParentStack&& stack) {
    // pop leaf page and separator index
    PageGuard page = std::move(stack.back().page);
    SlotIndex index = stack.back().index;
    stack.pop_back();

    // calculate deficit
    uint16_t deficit = page.minOccupancy() - usedSpace(page);

    // fetch siblings if they exist
    std::optional<BTree::Sibling> leftSibling;
    ASSIGN_OR_RETURN(leftSibling, getLeftSibling(stack.back().page, index));
    std::optional<BTree::Sibling> rightSibling;
    ASSIGN_OR_RETURN(rightSibling, getRightSibling(stack.back().page, index));

    // try to redistribute and return if successful
    if (leftSibling && leftSibling->surplus >= deficit) {
        if (shiftRight(page, leftSibling->page, stack.back().page, index)) {
            return Ok();
        }
    }
    if (rightSibling && rightSibling->surplus >= deficit) {
        if (shiftLeft(page, rightSibling->page, stack.back().page, index)) {
            return Ok();
        }
    }

    // try to merge and return if not possible
    if (leftSibling && leftSibling->surplus <= deficit) {
        RETURN_ON_ERROR(mergeLeaf(leftSibling->page, page, stack.back().page, index));
    } else if (rightSibling && rightSibling->surplus <= deficit) {
        RETURN_ON_ERROR(mergeLeaf(page, rightSibling->page, stack.back().page, index));
    } else {
        // if neither redistribution nor merging is possible, leave page underfilled
        return Ok();
    }

    // unwind the parent stack and check if parent page now has a deficit

}

bool BTree::shift(
    PageGuard& page,
    PageGuard& sibling, 
    PageGuard& parent,
    SlotIndex separatorIndex,
    bool fromLeft
) {
    // check if the last entry of the left page is big enough to fill the
    // deficit without giving the left page a deficit
    uint16_t pageFreeSpace = freeSpace(page);
    uint16_t deficit = page.minOccupancy() - pageFreeSpace;
    int16_t surplus = surplusSpace(sibling);
    SlotIndex entryIndex = fromLeft
                           ? sibling.getHeader().slotCount - 1
                           : 0;
    uint16_t entrySize = entryFootprint(sibling, entryIndex);
    if (entrySize < deficit                 // entry not big enough to remove deficit
        || entrySize > surplus              // moving entry will give sibling deficit
        || entrySize > pageFreeSpace) {     // entry too big for page
        return false;
    }

    // check if the parent page has enough space for the new separator key
    auto oldSeparatorSize = entryFootprint(parent, separatorIndex);
    auto newSeparatorSize = entryFootprint(sibling, entryIndex);
    auto parentFreeSpace = freeSpace(parent);
    if (newSeparatorSize > oldSeparatorSize
        && newSeparatorSize - oldSeparatorSize > parentFreeSpace) {
        return false;
    }

    // move entry from sibling to page
    insertIntoLeaf(
        page,
        (fromLeft ? 0 : page.getHeader().slotCount),
        getKey(sibling, entryIndex),
        getValue(sibling, entryIndex)
    );
    deleteEntry(sibling, entryIndex);
    
    // remove old separator key from parent and insert new separator key
    deleteEntry(parent, separatorIndex);
    insertIntoInner(
        parent,
        separatorIndex,
        getKey((fromLeft ? page : sibling), 0),
        (fromLeft ? sibling.pageNum() : page.pageNum())
    );

    return true;
}

// must check that left sibling exists before attempting to take from left
bool BTree::shiftRight(
    PageGuard& page,
    PageGuard& sibling,
    PageGuard& parent,
    SlotIndex index
) {
    return shift(
        page,
        sibling,
        parent,
        index - 1,
        true
    );
}

bool BTree::shiftLeft(
    PageGuard& page,
    PageGuard& sibling, 
    PageGuard& parent,
    SlotIndex index
) {
    return shift(
        page,
        sibling,
        parent,
        index,
        false
    );
}

bool BTree::rotate(
    PageGuard& page,
    PageGuard& sibling, 
    PageGuard& parent,
    SlotIndex separatorIndex,
    bool fromLeft
) {
    // check if separator entry fits in page
    uint16_t pageFreeSpace = freeSpace(page);
    uint16_t deficit = page.minOccupancy() - pageFreeSpace;
    uint16_t separatorSize = entryFootprint(parent, separatorIndex);
    if (separatorSize < deficit || separatorSize > pageFreeSpace) {
        return false;
    }

    // check if donor entry fits in parent and if moving donor
    // will cause siblin got underflow
    SlotIndex donorIndex = fromLeft
                           ? sibling.getHeader().slotCount - 1
                           : 0;
    uint16_t donorSize = entryFootprint(sibling, donorIndex);
    if ((donorSize > separatorSize && donorSize - separatorSize > freeSpace(parent))
        || usedSpace(sibling) - donorSize < sibling.minOccupancy()) {
        return false;
    }

    // insert new entry into page
    PageGuard& leftPage = (fromLeft ? sibling : page);
    insertIntoInner(
        page,
        (fromLeft ? 0 : page.getHeader().slotCount),
        getKey(parent, separatorIndex),
        leftPage.getHeader().next
    );

    // update separator key
    auto newSeparatorKey = getKey(sibling, donorIndex);
    deleteEntry(parent, separatorIndex);
    insertIntoInner(
        parent,
        separatorIndex,
        newSeparatorKey,
        leftPage.pageNum()
    );

    // update leftpage.next
    auto leftHeader = leftPage.getHeader();
    leftHeader.next = getChild(sibling, donorIndex);
    leftPage.setHeader(leftHeader);

    // delete donated entry
    deleteEntry(sibling, donorIndex);

    return true;
}

bool BTree::rotateRight(
    PageGuard& page,
    PageGuard& sibling, 
    PageGuard& parent,
    SlotIndex separatorIndex
) {
    return rotate(
        page,
        sibling,
        parent,
        separatorIndex - 1,
        true
    );
}

bool BTree::rotateLeft(
    PageGuard& page,
    PageGuard& sibling, 
    PageGuard& parent,
    SlotIndex separatorIndex
) {
    return rotate(
        page,
        sibling,
        parent,
        separatorIndex,
        false
    );
}

DbResult<void> BTree::mergeLeaf(
    PageGuard& leftPage,
    PageGuard& rightPage,
    PageGuard& parent,
    SlotIndex separatorIndex
) {
    // update leftPage.next
    auto leftHeader = leftPage.getHeader();
    auto rightHeader = rightPage.getHeader();
    leftHeader.next = rightHeader.next;
    leftPage.setHeader(leftHeader);

    // move all entries from rightPage to leftPage
    for (SlotIndex i = leftHeader.slotCount, j = 0; j < rightHeader.slotCount; ++i, ++j) {
        insertIntoLeaf(leftPage, i, getKey(rightPage, j), getValue(rightPage, j));
    }

    // update rightPage.next.prev if it exists
    if (rightHeader.next != INVALID_PAGE) {
        PageGuard nextPage;
        ASSIGN_OR_RETURN(nextPage, pager_.fetchPage(rightHeader.next));
        auto nextHeader = nextPage.getHeader();
        nextHeader.prev = leftPage.pageNum();
        nextPage.setHeader(nextHeader);
    }

    // delete separator key from parent
    deleteEntry(parent, separatorIndex);

    // release rightPage and delete
    auto rightPageNum = rightPage.pageNum();
    rightPage.release();
    RETURN_ON_ERROR(pager_.deletePage(rightPageNum));

    return Ok();
}

DbResult<bool> BTree::mergeInner(
    PageGuard& leftPage,
    PageGuard& rightPage,
    PageGuard& parent,
    SlotIndex separatorIndex
) {
    // check if leftPage has sufficient freeSpace
    auto leftFree = freeSpace(leftPage);
    auto rightused = usedSpace(rightPage);
    auto separatorSize = entryFootprint(parent, separatorIndex);
    if (rightused + separatorSize > leftFree) {
        return Ok(false);
    }

    // insert entry for separator key into leftpage
    auto leftHeader = leftPage.getHeader();
    insertIntoInner(leftPage, leftHeader.slotCount, getKey(parent, separatorIndex), leftHeader.next);

    // delete separator key from parent and update child pointer for next slot
    deleteEntry(parent, separatorIndex);
    // separatorIndex is now the next slot after deleting the separator entry
    setChild(parent, separatorIndex, leftPage.pageNum());

    // move all entries from rightPage to leftPage
    // adjusting slotcount after inserting the separator key
    auto rightHeader = rightPage.getHeader();
    for (SlotIndex i = leftHeader.slotCount + 1, j = 0; j < rightHeader.slotCount; ++i, ++j) {
        insertIntoInner(leftPage, i, getKey(rightPage, j), getChild(rightPage, j));
    }

    // update leftPage.next
    leftHeader = leftPage.getHeader(); // update stale leftHeader after insertion
    leftHeader.next = rightHeader.next;
    leftPage.setHeader(leftHeader);

    // release rightPage and delete
    auto rightPageNum = rightPage.pageNum();
    rightPage.release();
    RETURN_ON_ERROR(pager_.deletePage(rightPageNum));

    return Ok(true);
}

DbResult<std::optional<BTree::Sibling>> BTree::getLeftSibling(
    const PageGuard& parent,
    SlotIndex index
) {
    if (index == 0) {
        return Ok(std::optional<BTree::Sibling>());
    }

    auto pageNum = getChild(parent, index - 1);
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(pageNum));
    Sibling sibling{std::move(page), surplusSpace(page)};
    return Ok(std::optional<BTree::Sibling>(std::move(sibling)));
}

DbResult<std::optional<BTree::Sibling>> BTree::getRightSibling(
    const PageGuard& parent,
    SlotIndex index
) {
    if (index == parent.getHeader().slotCount) {
        return Ok(std::optional<BTree::Sibling>());
    }

    auto pageNum = getChild(parent, index + 1);
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(pageNum));
    Sibling sibling{std::move(page), surplusSpace(page)};
    return Ok(std::optional<BTree::Sibling>(std::move(sibling)));
}

} // namespace LiliumDB