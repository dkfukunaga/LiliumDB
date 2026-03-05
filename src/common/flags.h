#ifndef LILIUMDB_FLAGS_H
#define LILIUMDB_FLAGS_H

#include <type_traits>
#include <initializer_list>

namespace LiliumDB {

template<typename Enum>
class Flags {
    static_assert(std::is_enum_v<Enum>, "Flags<Enum> requires an enum type");

public:
    using EnumType = Enum;
    using Int = std::underlying_type_t<Enum>;

    Flags() : flags_(0) {}
    Flags(Enum flag) : flags_(static_cast<Int>(flag)) {}
    Flags(std::initializer_list<Enum> flags) : flags_(0) {
        for (Enum f : flags) {
            flags_ |= static_cast<Int>(f);
        }
    }

    Flags& set(Enum flag, bool on = true) {
        if (on) {
            flags_ |= static_cast<Int>(flag);
        } else {
            flags_ &= ~static_cast<Int>(flag);
        }
        return *this;
    }
    Flags& setAll(std::initializer_list<Enum> flags, bool on = true) {
        for (Enum f : flags) {
            set(f, on);
        }
        return *this;
    }

    bool has(Enum flag) const {
        return (flags_ & static_cast<Int>(flag)) != 0;
    }
    bool hasAny(std::initializer_list<Enum> flags) const {
        for (Enum f : flags) {
            if ((flags_ & static_cast<Int>(f)) != 0) {
                return true;
            }
        }
        return false;
    }
    bool hasAny(Flags flags) const {
        return (flags_ & flags.flags_) != 0;
    }
    bool hasAll(std::initializer_list<Enum> flags) const {
        for (Enum f : flags) {
            if ((flags_ & static_cast<Int>(f)) == 0) {
                return false;
            }
        }
        return true;
    }
    bool hasAll(Flags flags) const {
        return (flags_ & flags.flags_) == flags.flags_;
    }
    

    Flags operator|(Enum flag) const {
        return Flags(flags_ | static_cast<Int>(flag));
    }
    Flags operator|(Flags flag) const {
        return Flags(flags_ | flag.flags_);
    }
    Flags operator&(Enum flag) const {
        return Flags(flags_ & static_cast<Int>(flag));
    }
    Flags operator&(Flags flag) const {
        return Flags(flags_ & flag.flags_);
    }
    Flags operator~() const {
        return Flags(~flags_);
    }
    bool operator==(Flags other) const {
        return flags_ == other.flags_;
    }
    bool operator!=(Flags other) const {
        return flags_ != other.flags_;
    }
    bool operator!() const {
        return flags_ == 0;
    }

    Flags& operator|=(Enum flag) {
        flags_ |= static_cast<Int>(flag);
        return *this;
    }
    Flags& operator|=(Flags flag) {
        flags_ |= flag.flags_;
        return *this;
    }
    Flags& operator&=(Enum flag) {
        flags_ &= static_cast<Int>(flag);
        return *this;
    }
    Flags& operator&=(Flags flag) {
        flags_ &= flag.flags_;
        return *this;
    }

    explicit operator bool() const {
        return flags_ != 0;
    }
    operator Int() const {
        return flags_;
    }

private:
    explicit Flags(Int flags) : flags_(flags) {}
    Int flags_;
};

} // namespace LiliumDB

#endif