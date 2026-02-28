#ifndef LILIUMDB_TYPES_H
#define LILIUMDB_TYPES_H

#include <cstdint>

namespace LiliumDB {

using PageNum = uint32_t;
using SlotNum = uint16_t;
using FileOffset = uint64_t;
using PageOffset = uint16_t;

struct RID {
    PageNum page;
    SlotNum slot;

    bool operator==(const RID& other) const {
        return this->page == other.page && this->slot == other.slot;
    }
    bool operator!=(const RID& other) const {
        return !(*this == other);
    }
};

class Status {
public:
    enum Code { Ok, ErrNotFound, ErrIO, ErrCorrupt } code;

    Status(Code c, const char* msg = nullptr): code(c), message_(msg) { }

    explicit operator bool() const { return code == Ok; }

    const char* message() const { return message_ == nullptr ? "" : message_; }

private:
    const char* message_;

};

} // namespace LiliumDB

#endif