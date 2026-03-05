#ifndef LILIUMDB_FLAGS_H
#define LILIUMDB_FLAGS_H

#include <type_traits>
#include <initializer_list>
#include <string>

namespace LiliumDB {

template<typename Enum>
class Flags {
    static_assert(std::is_enum_v<Enum>, "Flags<Enum> requires an enum type");

public:
    using EnumType = Enum;
    using Int = std::underlying_type_t<Enum>;

    Flags() noexcept : flags_(0) {}
    Flags(Enum flag) noexcept : flags_(toInt(flag)) {}
    Flags(std::initializer_list<Enum> flags) noexcept : flags_(0) {
        for (Enum f : flags) {
            flags_ |= toInt(f);
        }
    }

    Flags& set(Enum flag, bool on = true) noexcept {
        if (on) {
            flags_ |= toInt(flag);
        } else {
            flags_ &= ~toInt(flag);
        }
        return *this;
    }
    Flags& setAll(std::initializer_list<Enum> flags, bool on = true) noexcept {
        for (Enum f : flags) {
            set(f, on);
        }
        return *this;
    }
    Flags& setAll(Flags flags, bool on = true) noexcept {
        if (on) {
            flags_ |= flags.flags_;
        } else {
            flags_ &= ~flags.flags_;
        }
        return *this;
    }
    Flags& toggle(Enum flag) noexcept {
        flags_ ^= toInt(flag);
        return *this;
    }
    Flags& remove(Enum flag) noexcept {
        set(flag, false);
        return *this;
    }
    Flags& clear() noexcept {
        flags_ = 0;
        return *this;
    }
    

    bool has(Enum flag) const noexcept {
        return (flags_ & toInt(flag)) != 0;
    }
    bool hasAny(std::initializer_list<Enum> flags) const noexcept {
        for (Enum f : flags) {
            if ((flags_ & toInt(f)) != 0) {
                return true;
            }
        }
        return false;
    }
    bool hasAny(Flags flags) const noexcept {
        return (flags_ & flags.flags_) != 0;
    }
    bool hasAll(std::initializer_list<Enum> flags) const noexcept {
        for (Enum f : flags) {
            if ((flags_ & toInt(f)) == 0) {
                return false;
            }
        }
        return true;
    }
    bool hasAll(Flags flags) const noexcept {
        return (flags_ & flags.flags_) == flags.flags_;
    }

    std::string toBitString() const {
        using UInt = std::make_unsigned_t<Int>;
        constexpr int width = int(sizeof(Int) * 8);

        std::string str;
        str.reserve(width);

        UInt u = static_cast<UInt>(flags_);
        for (int i = width - 1; i >= 0; --i) {
            str += (u & (UInt(1) << i)) ? '1' : '0';
            if (i % 4 == 0 && i != 0) str += ' ';
        }
        return str;
    }

    Int bits() const noexcept { return flags_; }
    explicit operator Int() const noexcept { return flags_; }
    explicit operator bool() const noexcept { return flags_ != 0; }

    Flags operator~() const noexcept { return Flags(~flags_); }
    Flags& operator|=(Enum other) noexcept { flags_ |= toInt(other); return *this; }
    Flags& operator|=(Flags other) noexcept { flags_ |= other.flags_; return *this; }
    Flags& operator&=(Enum other) noexcept { flags_ &= toInt(other); return *this; }
    Flags& operator&=(Flags other) noexcept { flags_ &= other.flags_; return *this; }
    Flags& operator^=(Enum other) noexcept { flags_ ^= toInt(other); return *this; }
    Flags& operator^=(Flags other) noexcept { flags_ ^= other.flags_; return *this; }

    friend bool operator==(Flags lhs, Flags rhs) noexcept { return lhs.flags_ == rhs.flags_; }
    friend bool operator!=(Flags lhs, Flags rhs) noexcept { return lhs.flags_ != rhs.flags_; }

    friend Flags operator|(Flags lhs, Flags rhs) noexcept { lhs |= rhs; return lhs; }
    friend Flags operator|(Flags lhs, Enum rhs) noexcept { lhs |= rhs; return lhs; }
    friend Flags operator|(Enum lhs, Flags rhs) noexcept { return rhs | lhs; }

    friend Flags operator&(Flags lhs, Flags rhs) noexcept { lhs &= rhs; return lhs; }
    friend Flags operator&(Flags lhs, Enum rhs) noexcept { lhs &= rhs; return lhs; }
    friend Flags operator&(Enum lhs, Flags rhs) noexcept { return rhs & lhs; }

    friend Flags operator^(Flags lhs, Flags rhs) noexcept { lhs ^= rhs; return lhs; }
    friend Flags operator^(Flags lhs, Enum rhs) noexcept { lhs ^= rhs; return lhs; }
    friend Flags operator^(Enum lhs, Flags rhs) noexcept { return rhs ^ lhs; }


private:
    explicit Flags(Int flags) noexcept : flags_(flags) {}

    Int flags_;

    static constexpr Int toInt(Enum flag) noexcept { return static_cast<Int>(flag); }
};

} // namespace LiliumDB

#endif