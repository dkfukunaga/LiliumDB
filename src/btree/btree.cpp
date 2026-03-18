#include "btree.h"

#include "common/db_result.h"
#include "btree_page.h"

#include <algorithm>

namespace LiliumDB {

DbResult<void> BTree::insert(ByteView key, ByteView value) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, leafSearch(key));

    PagePosition pagePosition = std::move(stack.back());
    PageGuard page = std::move(pagePosition.page);
    SlotIndex index = pagePosition.slot;
    PageHeader header = page.getHeader();

    uint16_t freeSpace = static_cast<uint16_t>(
        (PAGE_END_OFFSET - header.slotCount * sizeof(Slot)) - header.freeOffset);
    uint16_t insertSize = static_cast<uint16_t>(
        key.size() + value.size() + sizeof(Slot) + sizeof(KeyValueHeader));

    if (insertSize <= freeSpace) {
        insertIntoLeaf(page, index, key, value);
    } else {
        RETURN_ON_ERROR(splitAndInsert(std::move(page), index, std::move(stack), key, value));
    }

    return Ok();
}

DbResult<void> BTree::remove(ByteView key) {
    
}

DbResult<Cursor> BTree::seek(ByteView key) {
    BTree::ParentStack stack;
    ASSIGN_OR_RETURN(stack, leafSearch(key));
    PagePosition pagePosition = std::move(stack.back());
    stack.pop_back();

    return Ok(Cursor(pager_, std::move(pagePosition.page), pagePosition.slot));
}

DbResult<Cursor> BTree::first() {
    
}

DbResult<Cursor> BTree::last() {

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

void BTree::insertIntoLeaf(PageGuard& page, SlotIndex index, ByteView key, ByteView value) {
    
}

void BTree::insertIntoInternal(PageGuard& page, SlotIndex index, ByteView key, PageNum child) {
    
}

DbResult<void> BTree::splitAndInsert(PageGuard page, SlotIndex index, ParentStack&& stack, ByteView key, ByteView value) {

}

} // namespace LiliumDB