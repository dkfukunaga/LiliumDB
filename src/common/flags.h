#ifndef LILIUMDB_FLAGS_H
#define LILIUMDB_FLAGS_H

#include <type_traits>
#include <initializer_list>
#include <string>

namespace LiliumDB {

// Type-safe bitmask wrapper over an enum type.
// Enum must be an enum (plain or enum class) with an explicit unsigned underlying
// type. Stores flags in the enum's underlying integer type with no overhead.
//
// Typical usage:
//   enum class FileFlag : uint16_t { ReadOnly = 0x01, Corrupt = 0x02 };
//   using FileFlags = Flags<FileFlag>;
//
//   FileFlags f = { FileFlag::ReadOnly, FileFlag::Corrupt };
//   if (f.has(FileFlag::ReadOnly)) { ... }
template<typename Enum>
class Flags {
    static_assert(std::is_enum_v<Enum>, "Flags<Enum> requires an enum type");

public:
    using EnumType = Enum;
    using Int = std::underlying_type_t<Enum>;

    // --- Constructors ---

    /// Constructs an empty Flags with no bits set.
    Flags() noexcept : flags_(0) {}
    /// Constructs a Flags with a single flag set.
    Flags(Enum flag) noexcept : flags_(toInt(flag)) {}
    /// Constructs a Flags with all flags in the initializer list set.
    Flags(std::initializer_list<Enum> flags) noexcept : flags_(0) {
        for (Enum f : flags) {
            flags_ |= toInt(f);
        }
    }

    // --- Mutators ---

    /// Sets or clears a single flag depending on `on`.
    Flags& set(Enum flag, bool on = true) noexcept {
        if (on) {
            flags_ |= toInt(flag);
        } else {
            flags_ &= ~toInt(flag);
        }
        return *this;
    }
    /// Sets or clears each flag in the list depending on `on`.
    Flags& setAll(std::initializer_list<Enum> flags, bool on = true) noexcept {
        for (Enum f : flags) {
            set(f, on);
        }
        return *this;
    }
    /// Sets or clears all flags in `flags` depending on `on`.
    Flags& setAll(Flags flags, bool on = true) noexcept {
        if (on) {
            flags_ |= flags.flags_;
        } else {
            flags_ &= ~flags.flags_;
        }
        return *this;
    }
    /// Toggles a single flag.
    Flags& toggle(Enum flag) noexcept {
        flags_ ^= toInt(flag);
        return *this;
    }
    /// Clears a single flag.
    Flags& remove(Enum flag) noexcept {
        set(flag, false);
        return *this;
    }
    /// Clears all flags.
    Flags& clear() noexcept {
        flags_ = 0;
        return *this;
    }

    // --- Queries ---

    /// Returns true if the given flag is set.
    bool has(Enum flag) const noexcept {
        return (flags_ & toInt(flag)) != 0;
    }
    /// Returns true if any flag in the list is set.
    /// Returns false if the list is empty.
    bool hasAny(std::initializer_list<Enum> flags) const noexcept {
        for (Enum f : flags) {
            if ((flags_ & toInt(f)) != 0) {
                return true;
            }
        }
        return false;
    }
    /// Returns true if any flag in `flags` is set.
    bool hasAny(Flags flags) const noexcept {
        return (flags_ & flags.flags_) != 0;
    }
    /// Returns true if all flags in the list are set.
    /// Returns true if the list is empty (vacuously true).
    bool hasAll(std::initializer_list<Enum> flags) const noexcept {
        for (Enum f : flags) {
            if ((flags_ & toInt(f)) == 0) {
                return false;
            }
        }
        return true;
    }
    /// Returns true if all flags in `flags` are set.
    bool hasAll(Flags flags) const noexcept {
        return (flags_ & flags.flags_) == flags.flags_;
    }

    // --- Conversions ---

    /// Returns the raw underlying integer value.
    Int bits() const noexcept { return flags_; }
    /// Explicit conversion to the underlying integer type.
    explicit operator Int() const noexcept { return flags_; }
    /// Returns true if any flag is set.
    explicit operator bool() const noexcept { return flags_ != 0; }

    /// Returns a binary string representation of the flags, grouped in nibbles.
    /// e.g. "0000 0000 0000 0011"
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

    // --- Operators ---

    /// Returns the bitwise complement of this Flags.
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