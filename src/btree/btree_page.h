#ifndef LILIUMDB_BTREE_TYPES_H
#define LILIUMDB_BTREE_TYPES_H

#include "common/types.h"
#include "common/file_format.h"
#include "utils/byte_span.h"
#include "pager/page_guard.h"

namespace LiliumDB {

inline constexpr uint16_t SLOT_SIZE = 4;
inline constexpr uint16_t KEY_HEADER_SIZE = 6;
inline constexpr uint16_t KEY_VALUE_HEADER_SIZE = 4;
inline constexpr uint16_t KEY_SLOT_OVERHEAD = SLOT_SIZE + KEY_HEADER_SIZE;
inline constexpr uint16_t KEY_VALUE_SLOT_OVERHEAD = SLOT_SIZE + KEY_VALUE_HEADER_SIZE;

struct Slot {
    PageOffset  offset;
    uint16_t    size;
};

static_assert(sizeof(Slot) == SLOT_SIZE);

#pragma pack(push, 1)
struct KeyHeader {
    uint16_t    keySize;
    PageNum     childPage;
    // key bytes follow
};
#pragma pack(pop)

static_assert(sizeof(KeyHeader) == KEY_HEADER_SIZE);

struct KeyValueHeader {
    uint16_t    keySize;
    uint16_t    valueSize;
    // key bytes follow
    // then value bytes follow
};

static_assert(sizeof(KeyValueHeader) == KEY_VALUE_HEADER_SIZE);

} // namespace LiliumDB

namespace LiliumDB::BTreePage {

inline PageOffset slotOffset(SlotIndex index) noexcept {
    return static_cast<PageOffset>(PAGE_END_OFFSET - (index + 1) * sizeof(Slot));
}
inline PageOffset slotOffset(int index) noexcept {
    return slotOffset(static_cast<SlotIndex>(index));
}

ByteView getKey(const PageGuard& page, SlotIndex index);
ByteView getValue(const PageGuard& page, SlotIndex index);
PageNum getChild(const PageGuard& page, SlotIndex index);
void setChild(PageGuard& page, SlotIndex index, PageNum child);

inline uint16_t freeSpace(const PageGuard& page) {
    auto header = page.getHeader();
    return slotOffset(header.slotCount - 1) - header.freeOffset;
}

inline uint16_t usedSpace(const PageGuard& page) {
    return page.usableSize() - freeSpace(page);
}

} // namespace LiliumDB::BTreePage

#endif