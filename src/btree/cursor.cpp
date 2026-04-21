#include "cursor.h"

using namespace LiliumDB::BTreePage;

namespace LiliumDB {

DbResult<void> Cursor::next() {
    PageHeader header = page_.getHeader();

    if (header.slotCount > 0 && slot_ < header.slotCount - 1) {
        slot_++;
    } else { // at last slot of the page
        if (header.next == INVALID_PAGE) {
            invalidate();
        } else {
            ASSIGN_OR_RETURN(page_, pager_->fetchPage(header.next));
            header = page_.getHeader();

            if (header.slotCount == 0) {
                invalidate();
            } else {
                slot_ = 0;
            }
        }
    }

    return Ok();
}

DbResult<void> Cursor::prev() {
    PageHeader header = page_.getHeader();

    if (slot_ > 0) {
        slot_--;
    } else { // at first slot of the page
        if (header.prev == INVALID_PAGE) {
            invalidate();
        } else {
            ASSIGN_OR_RETURN(page_, pager_->fetchPage(header.prev));
            header = page_.getHeader();

            if (header.slotCount == 0) {
                invalidate();
            } else {
                slot_ = header.slotCount - 1;
            }
        }
    }

    return Ok();
}

} // namespace LiliumDB