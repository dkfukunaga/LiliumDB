#include "btree_page.h"

namespace LiliumDB {

ByteView getKey(const PageGuard& page, SlotIndex index) {
    Slot slot = page.view().get<Slot>(slotOffset(index));
    PageHeader header = page.getHeader();
    ByteView key;

    if (header.treeLevel == 0) {
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

    assert(header.treeLevel == 0);
    if (header.treeLevel > 0) return ByteView();

    auto keyValueHeader = page.view().get<KeyValueHeader>(slot.offset);
    return page.subview(
        static_cast<PageOffset>(slot.offset + sizeof(KeyValueHeader) + keyValueHeader.keySize),
        keyValueHeader.valueSize);
}

PageNum getChild(const PageGuard& page, SlotIndex index) {
    Slot slot = page.view().get<Slot>(slotOffset(index));
    PageHeader header = page.getHeader();
    assert(header.treeLevel > 0);

    if (header.treeLevel == 0) return INVALID_PAGE;

    auto keyHeader = page.view().get<KeyHeader>(slot.offset);
    return keyHeader.childPage;
}

} // namespace LiliumDB
