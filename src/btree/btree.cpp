#include "btree.h"

#include <algorithm>

#include "btree_page.h"

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

    return Ok();
}

DbResult<bool> BTree::remove(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, search(key));

    PageGuard page = std::move(stack.back().page);
    SlotIndex index = stack.back().index;
    stack.pop_back();

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

    // remove slot and adjust over slots in the slot table
    auto header = page.getHeader();
    for (SlotIndex i = index + 1; i < header.slotCount; ++i) {
        Slot slot = page.view().get<Slot>(slotOffset(i));
        page.span().put<Slot>(slotOffset(i - 1), slot);
    }

    // decrement slot count
    header.slotCount--;
    page.setHeader(header);

    // call rebalance if page is now under minimum occupancy

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
    auto slotCount = header.slotCount;
    ParentStack stack;
    SlotIndex index = INVALID_SLOT;

    if (slotCount == 0) {
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
    SlotIndex mid;
    SlotIndex low = 0;
    SlotIndex high = page.getHeader().slotCount - 1;

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
        slotBufSpan.put<Slot>(slotBufOffset, slot);

        cellBufOffset += slot.size;
        assert(slotBufOffset >= sizeof(Slot));
        slotBufOffset = static_cast<uint16_t>(slotBufOffset - sizeof(Slot));
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

void BTree::insertIntoInner(PageGuard& page, SlotIndex index, ByteView key, PageNum child) {
    auto header = page.getHeader();
    auto cellOffset = header.freeOffset;
    uint16_t totalSize = static_cast<uint16_t>(sizeof(KeyHeader) + key.size());

    // write key-value cell with header
    KeyHeader keyHeader{static_cast<uint16_t>(key.size()), child};
    page.span().put<KeyHeader>(cellOffset, keyHeader);
    page.span().write(cellOffset + sizeof(KeyHeader), key);

    // relocate slots to make space for new slot
    for (int i = header.slotCount - 1; i >= static_cast<int>(index); --i) {
        page.span().copy_within(slotOffset(i), slotOffset(i + 1), sizeof(Slot));
    }
    page.span().put<Slot>(slotOffset(index), Slot{cellOffset, totalSize});

    // update page header
    header.slotCount++;
    header.freeOffset += totalSize;
    page.setHeader(header);
}

DbResult<void> BTree::splitAndInsert(
    ParentStack&& stack,
    ByteView key,
    ByteView value) {

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

        // check if key will fit
        uint16_t freeSpace = static_cast<uint16_t>(
            (PAGE_END_OFFSET - header.slotCount * sizeof(Slot)) - header.freeOffset);
        uint16_t insertSize = static_cast<uint16_t>(
            splitResult.key.size() + sizeof(PageNum) + sizeof(Slot) + sizeof(KeyHeader));

        // find index for separator key
        index = searchInner(page, ByteView(splitResult.key));

        // if the separator key fits, insert and exit loop
        // else, split and continue root
        if (freeSpace >= insertSize) {
            insertIntoInner(page, index, ByteView(splitResult.key), splitResult.child);
            needsRootPromotion = false;
            break;
        } else if (!stack.empty()) {
            ASSIGN_OR_RETURN(splitResult, splitInner(page, index, splitResult.key, splitResult.child));
        }
    }

    // if the stack is empty, we've reached the root and need to promote the left page to a new root page
    if (needsRootPromotion && stack.empty()) {
        RETURN_ON_ERROR(promoteRoot(page, ByteView(splitResult.key)));
    }

    return Ok();
}

DbResult<BTree::SplitResult> BTree::splitLeaf(
    PageGuard& leftPage,
    SlotIndex index,
    ByteView key,
    ByteView value) {
    // helper lambdas

    // returns total size of an entry and its slot
    auto logicalSize = [&](SlotIndex logical) -> uint16_t {
        if (logical == index)
            return static_cast<uint16_t>(KEY_VALUE_SLOT_OVERHEAD + key.size() + value.size());
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        return KEY_VALUE_SLOT_OVERHEAD + s.size;
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
    
    // if leftPage.next pointed to a valid next page, update nextpage.prev
    // to point to rightPage
    if (leftHeader.next != INVALID_PAGE) {
        PageGuard nextPage;
        ASSIGN_OR_RETURN(nextPage, pager_.fetchPage(leftHeader.next));
        auto nextHeader = nextPage.getHeader();
        nextHeader.prev = rightPage.pageNum();
        nextPage.setHeader(nextHeader);
    }

    // update header for leftPage and compact entries
    leftHeader.next = rightPage.pageNum();
    leftHeader.slotCount = splitIndex;
    leftPage.setHeader(leftHeader);
    compactPage(leftPage);

    return Ok(SplitResult{separatorKey, rightPage.pageNum()});
}

DbResult<BTree::SplitResult> BTree::splitInner(
    PageGuard& leftPage,
    SlotIndex index,
    std::vector<uint8_t> key,
    PageNum child) {
    // helper lambdas

    // returns total size of an entry and its slot
    auto logicalSize = [&](SlotIndex logical) -> uint16_t {
        if (logical == index)
            return static_cast<uint16_t>(KEY_SLOT_OVERHEAD + key.size() + sizeof(PageNum));
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        return KEY_SLOT_OVERHEAD + s.size;
    };

    struct Entry { std::vector<uint8_t> key; PageNum child; };

    // returns vectors of an entry's key and child page, slotting the new entry
    // into its logical place in the sequence of slots
    auto logicalEntry = [&](SlotIndex logical) -> Entry {
        if (logical == index)
            return { key, child };
        SlotIndex physical = logical < index ? logical : logical - 1;
        Slot s = leftPage.view().get<Slot>(slotOffset(physical));
        KeyHeader kh = leftPage.view().get<KeyHeader>(s.offset);
        return {
            leftPage.view().readAsVector(s.offset + KEY_HEADER_SIZE, kh.keySize),
            kh.childPage
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
    rightHeader.level = leftHeader.level;
    // slotCount and freeOffset updated by insertIntoInner()
    rightPage.setHeader(rightHeader);

    // insert entries from the split entry onwards into rightPage
    for (SlotIndex i = splitIndex + 1; i < slotCount + 1; ++i) {
        Entry entry = logicalEntry(i);
        insertIntoInner(rightPage, static_cast<SlotIndex>(i - splitIndex - 1), ByteView(entry.key), entry.child);
    }

    // update header for leftPage and compact entries
    leftHeader.slotCount = splitIndex;
    leftPage.setHeader(leftHeader);
    compactPage(leftPage);

    return Ok(SplitResult{separatorKey, rightPage.pageNum()});
}

DbResult<void> BTree::promoteRoot(
    PageGuard& rootPage,
    ByteView key) {
    // allocate a new left page and copy old left page into it
    PageGuard leftPage;
    ASSIGN_OR_RETURN(leftPage, pager_.newPage(type_));
    memcpy(leftPage.span().data(), rootPage.view().data(), PAGE_SIZE);

    // reset root page to an empty page and increment its level
    auto rootHeader = rootPage.getHeader();
    rootHeader.level++;
    rootHeader.slotCount = 0;
    rootHeader.freeOffset = rootPage.usableStart();
    rootHeader.prev = INVALID_PAGE;
    rootPage.setHeader(rootHeader);

    // insert separator key
    insertIntoInner(rootPage, 0, key, leftPage.pageNum());

    return Ok();
}

DbResult<void> BTree::rebalance(ParentStack&& stack) {
    
}

DbResult<BTree::MergeResult> BTree::mergeLeaf(PageGuard& leftPage, PageGuard& rightPage) {

}

DbResult<BTree::MergeResult> BTree::mergeInner(PageGuard& leftPage, PageGuard& rightPage) {

}

} // namespace LiliumDB