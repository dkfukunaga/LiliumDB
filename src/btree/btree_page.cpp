#include "btree_page.h"

namespace LiliumDB::BTreePage {

ByteView getKey(const PageGuard& page, SlotIndex index) {
    Slot slot = page.view().get<Slot>(slotOffset(index));
    PageHeader header = page.getHeader();
    ByteView key;

    if (header.level == 0) {
        auto keyValueHeader = page.view().get<KeyValueHeader>(slot.offset);
        key = page.subview(slot.offset + sizeof(KeyValueHeader), keyValueHeader.keySize);
    } else {
        auto keyHeader = page.view().get<KeyHeader>(slot.offset);
        key = page.subview(slot.offset + sizeof(KeyHeader), keyHeader.keySize);
    }

    return key;
}

ByteView getValue(const PageGuard& page, SlotIndex index) {
    Slot slot = page.view().get<Slot>(slotOffset(index));
    PageHeader header = page.getHeader();

    assert(header.level == 0);
    if (header.level > 0) return ByteView();

    auto keyValueHeader = page.view().get<KeyValueHeader>(slot.offset);
    return page.subview(
        static_cast<PageOffset>(slot.offset + sizeof(KeyValueHeader) + keyValueHeader.keySize),
        keyValueHeader.valueSize);
}

PageNum getChild(const PageGuard& page, SlotIndex index) {
    
    PageHeader header = page.getHeader();

    assert(header.level > 0);
    assert(index <= header.slotCount);

    if (header.slotCount == 0) {    // empty tree
        return INVALID_PAGE;
    } else if (index == header.slotCount) {
        return header.next;
    } else {
        Slot slot = page.view().get<Slot>(slotOffset(index));
        return page.view().get<KeyHeader>(slot.offset).childPage;
    }
}

void setChild(PageGuard& page, SlotIndex index, PageNum child) {
    
    PageHeader header = page.getHeader();

    assert(header.level > 0);
    assert(index <= header.slotCount);

    if (index == header.slotCount) {
        header.next = child;
        page.setHeader(header);
    } else {
        Slot slot = page.view().get<Slot>(slotOffset(index));
        page.span().put<PageNum>(slot.offset + offsetof(KeyHeader, childPage), child);
    }
}

} // namespace LiliumDB::BTreePage
