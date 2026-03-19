#include "btree.h"

#include <algorithm>

#include "btree_page.h"

namespace LiliumDB {

DbResult<void> BTree::insert(ByteView key, ByteView value) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, leafSearch(key));

    PagePosition pagePosition = std::move(stack.back());
    stack.pop_back();
    PageGuard page = std::move(pagePosition.page);
    SlotIndex index = pagePosition.slot;
    PageHeader header = page.getHeader();

    uint16_t freeSpace = static_cast<uint16_t>(
        (PAGE_END_OFFSET - header.slotCount * sizeof(Slot)) - header.freeOffset);
    uint16_t insertSize = static_cast<uint16_t>(
        key.size() + value.size() + sizeof(Slot) + sizeof(KeyValueHeader));

    if (insertSize > freeSpace) {
        freeSpace = compact(page);
    }

    if (insertSize <= freeSpace) {
        insertIntoLeaf(page, index, key, value);
    } else {
        RETURN_ON_ERROR(splitAndInsert(std::move(page), std::move(stack), index, key, value));
    }

    return Ok();
}

DbResult<void> BTree::remove(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, leafSearch(key));

    PagePosition pagePosition = std::move(stack.back());
    stack.pop_back();
    PageGuard page = std::move(pagePosition.page);
    SlotIndex index = pagePosition.slot;
    auto slotKey = getKey(page, index);
    
    if (key.size() != slotKey.size() ||
        memcmp(key.data(), slotKey.data(), key.size()) != 0) {
        // return key not found error
    }

    // remove slot and adjust over slots in the slot table
    // minimum occupancy is PAGE_USABLE_SIZE or PAGE_ZERO_USABLE_SIZE / 2
    // call mergeOrRedistribute if page is now under minimum occupancy
}

DbResult<Cursor> BTree::seek(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, leafSearch(key));
    PagePosition pagePosition = std::move(stack.back());
    stack.pop_back();

    return Ok(Cursor(&pager_, std::move(pagePosition.page), pagePosition.slot));
}

DbResult<Cursor> BTree::first() {
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(root_));
    auto header = page.getHeader();

    if (header.slotCount == 0) {
        return Ok(Cursor()); // invalid cursor
    }

    while (header.treeLevel > 0) {
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

    while (header.treeLevel > 0) {
        ASSIGN_OR_RETURN(page, pager_.fetchPage(header.next));
        header = page.getHeader();
    }

    return Ok(Cursor(&pager_, std::move(page), header.slotCount - 1));
}

DbResult<BTree::ParentStack> BTree::leafSearch(ByteView key) const {
    PageGuard page;
    ASSIGN_OR_RETURN(page, pager_.fetchPage(root_));
    PageHeader header = page.getHeader();
    ParentStack stack;
    SlotIndex index;

    while (header.treeLevel > 0) {
        PageNum child = header.next;
        auto slotCount = header.slotCount;

        if (slotCount == 0) {
            // follow rightmost child
        } else {
            // binary search
            SlotIndex low = 0;
            SlotIndex high = slotCount - 1;

            while (low <= high) {
                SlotIndex mid = static_cast<SlotIndex>(low + (high - low) / 2);
                auto slot = page.view().get<Slot>(slotOffset(mid));
                auto keyHeader = page.view().get<KeyHeader>(slot.offset);
                auto slotKey = page.subview(slot.offset + sizeof(KeyHeader), keyHeader.keySize);

                auto comp = memcmp(key.data(), slotKey.data(), std::min(key.size(), slotKey.size()));
                if (comp == 0) {
                    comp = (int) key.size() - (int) slotKey.size();
                }
                if (comp <= 0) {
                    child = keyHeader.childPage;
                    index = mid;
                    high = mid - 1;
                } else {
                    low = mid + 1;
                }
            }
        }
        stack.push_back({std::move(page), index});
        ASSIGN_OR_RETURN(page, pager_.fetchPage(child));
        header = page.getHeader();
    }
    stack.push_back({std::move(page), index});
    return Ok(std::move(stack));
}

uint16_t BTree::compact(PageGuard& page) {
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
    uint16_t slotBufOffset = slotBufSize - sizeof(Slot);
    for (int i = 0; i < header.slotCount; ++i) {
        // write cell to cell buffer
        Slot slot = page.view().get<Slot>(slotOffset(i));
        cellBuffSpan.write(cellBufOffset, page.subview(slot.offset, slot.size));

        // update slot and write to slot buffer
        slot.offset = start + cellBufOffset;
        slotBufSpan.put<Slot>(slotBufOffset, slot);

        cellBufOffset += slot.size;
        slotBufOffset -= sizeof(Slot);
    }

    // write compacted cell buffer and slot buffer to page
    page.span().write(start, cellBuffSpan);
    page.span().write(slotOffset(header.slotCount - 1), slotBufSpan);

    // update page header
    header.freeOffset = start + cellBufOffset;
    page.setHeader(header);

    // return total free space
    return slotOffset(header.slotCount) - header.freeOffset;
}

void BTree::insertIntoLeaf(PageGuard& page, SlotIndex index, ByteView key, ByteView value) {
    auto header = page.getHeader();
    auto cellOffset = header.freeOffset;
    uint16_t totalSize = static_cast<uint16_t>(sizeof(KeyValueHeader) + key.size() + value.size());

    // write key-value cell with header
    KeyValueHeader kvHeader{static_cast<uint16_t>(key.size()), static_cast<uint16_t>(value.size())};
    page.span().put<KeyValueHeader>(cellOffset, kvHeader);
    page.span().write(cellOffset + sizeof(KeyValueHeader), key);
    page.span().write(cellOffset + sizeof(KeyValueHeader) + key.size(), value);

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

DbResult<void> BTree::splitAndInsert(PageGuard page, ParentStack&& stack, SlotIndex index, ByteView key, ByteView value) {

}

DbResult<void> BTree::mergeOrRedistribute(PageGuard page, ParentStack&& stack) {
    // minimum occupancy is PAGE_USABLE_SIZE or PAGE_ZERO_USABLE_SIZE / 2
}

} // namespace LiliumDB