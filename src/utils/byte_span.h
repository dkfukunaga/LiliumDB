#ifndef LILIUMDB_BYTE_SPAN_H
#define LILIUMDB_BYTE_SPAN_H

#include <cstdint>      // uint8_t
#include <cstddef>      // size_t, offsetof
#include <cstring>      // memcpy, memmove
#include <stdexcept>    // std::out_of_range
#include <cassert>      // assert
#include <iterator>     // std::reverse_iterator
#include <vector>

namespace LiliumDB {

class ByteView;

// Non-owning mutable view over a contiguous byte buffer.
// Bounds-checked via exceptions; use operator[] for unchecked access.
// Does not manage lifetime — caller must ensure the buffer outlives this span.
class ByteSpan {
public:
    constexpr ByteSpan(): data_(nullptr), size_(0) { }
    constexpr ByteSpan(uint8_t* bytes, size_t size): data_(bytes), size_(size) {
        assert(data_ != nullptr || size_ == 0);
    }
    explicit ByteSpan(std::vector<uint8_t>& vec) : data_(vec.data()), size_(vec.size()) { }

    constexpr uint8_t&                  at(size_t offset);
    [[nodiscard]] constexpr ByteSpan    subspan(size_t start, size_t len);
    [[nodiscard]] constexpr ByteView    subview(size_t start, size_t len) const;

    // Writes sizeof(T) bytes of value at start. Throws if out of range.
    template <class T> void             put(size_t start, const T& value);
    // Copies len bytes from src into this span starting at start.
    void                                write(size_t start, const uint8_t* src, size_t len);
    void                                write(size_t start, const ByteView& src);
    // Copies len bytes within this span from [from, from+len) to [to, to+len).
    // Safe for overlapping regions.
    void                                copy_within(size_t to, size_t from, size_t len);
    void                                set(const uint8_t& value) { memset(data_, value, size_); }
    void                                clear() { set(0); }

    constexpr uint8_t*                  data() noexcept { return data_; }
    constexpr const uint8_t*            data() const noexcept { return data_; }
    constexpr size_t                    size() const noexcept { return size_; }

    constexpr uint8_t&                  operator[](size_t index) { return data_[index]; }
    constexpr const uint8_t&            operator[](size_t index) const { return data_[index]; }

    using iterator                      = uint8_t*;
    using const_iterator                = const uint8_t*;
    using reverse_iterator              = std::reverse_iterator<iterator>;
    using const_reverse_iterator        = std::reverse_iterator<const_iterator>;

    constexpr iterator                  begin()         { return data_; }
    constexpr iterator                  end()           { return data_ + size_; }
    constexpr const_iterator            begin()   const { return data_; }
    constexpr const_iterator            end()     const { return data_ + size_; }
    constexpr const_iterator            cbegin()  const { return data_; }
    constexpr const_iterator            cend()    const { return data_ + size_; }

    constexpr reverse_iterator          rbegin()        { return reverse_iterator(end()); }
    constexpr reverse_iterator          rend()          { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator    rbegin()  const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator    rend()    const { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator    crbegin() const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator    crend()   const { return const_reverse_iterator(begin()); }

private:
    uint8_t*    data_;
    size_t      size_;
};

// Non-owning read-only view over a contiguous byte buffer.
// Bounds-checked via exceptions; use operator[] for unchecked access.
// Implicitly constructible from ByteSpan.
// Does not manage lifetime — caller must ensure the buffer outlives this span.
class ByteView {
public:
    constexpr ByteView(): data_(nullptr), size_(0) { }
    constexpr ByteView(const uint8_t* bytes, size_t size): data_(bytes), size_(size) {
        assert(data_ != nullptr || size_ == 0);
    }
    constexpr ByteView(const ByteSpan& span): data_(span.data()), size_(span.size()) { }
    explicit ByteView(const std::vector<uint8_t>& vec) : data_(vec.data()), size_(vec.size()) { }

    constexpr uint8_t                   at(size_t offset) const;
    [[nodiscard]] constexpr ByteView    subview(size_t start, size_t len) const;
    std::vector<uint8_t>                toVector() const;

    // Reads sizeof(T) bytes at start from this view and returns as T. Throws if out of range.
    template <class T> T                get(size_t start) const;
    // Reads sizeof(T) bytes at start from this view and assigns to T. Throws if out of range.
    template <class T> void             get(size_t start, T& dst) const;
    // Copies len bytes from this view into dst starting at start.
    void                                read(size_t start, uint8_t* dst, size_t len) const;
    std::vector<uint8_t>                readAsVector(size_t start, size_t len) const;

    constexpr const uint8_t*            data() const noexcept { return data_; }
    constexpr size_t                    size() const noexcept { return size_; }

    constexpr const uint8_t&            operator[](size_t index) const { return data_[index]; }
    friend bool                         operator==(ByteView lhs, ByteView rhs);
    friend bool                         operator!=(ByteView lhs, ByteView rhs);

    using const_iterator                = const uint8_t*;
    using const_reverse_iterator        = std::reverse_iterator<const_iterator>;

    constexpr const_iterator            begin()   const { return data_; }
    constexpr const_iterator            end()     const { return data_ + size_; }
    constexpr const_iterator            cbegin()  const { return data_; }
    constexpr const_iterator            cend()    const { return data_ + size_; }

    constexpr const_reverse_iterator    rbegin()  const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator    rend()    const { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator    crbegin() const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator    crend()   const { return const_reverse_iterator(begin()); }

private:
    const uint8_t*  data_;
    size_t          size_;
};

// ByteSpan method definitions

constexpr uint8_t& ByteSpan::at(size_t offset) {
    if (offset < size_)
        return data_[offset];
    else
        throw std::out_of_range("offset out of range");
}

constexpr ByteSpan ByteSpan::subspan(size_t start, size_t len) {
    if (start <= size_ && len <= size_ - start)
        return ByteSpan(data_ + start, len);
    else
        throw std::out_of_range("subspan out of range");
}

constexpr ByteView ByteSpan::subview(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteView(data_ + start, len);
    else
        throw std::out_of_range("subview out of range");
}

template <class T>
inline void ByteSpan::put(size_t start, const T& value) {
    if (start <= size_ && sizeof(T) <= size_ - start)
        std::memcpy(data_ + start, &value, sizeof(T));
    else
        throw std::out_of_range("destination out of range");
}

inline void ByteSpan::write(size_t start, const uint8_t* src, size_t len) {
    if (start <= size_ && len <= size_ - start)
        std::memcpy(data_ + start, src, len);
    else
        throw std::out_of_range("destination out of range");
}

inline void ByteSpan::write(size_t start, const ByteView& src) {
    write(start, src.data(), src.size());
}

inline void ByteSpan::copy_within(size_t to, size_t from, size_t len) {
    if (to <= size_ && from <= size_ && len <= size_ - to && len <= size_ - from)
        std::memmove(data_ + to, data_ + from, len);
    else
        throw std::out_of_range("move out of range");
}

// ByteView method definitions

constexpr uint8_t ByteView::at(size_t offset) const {
    if (offset < size_)
        return data_[offset];
    else
        throw std::out_of_range("offset out of range");
}

constexpr ByteView ByteView::subview(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteView(data_ + start, len);
    else
        throw std::out_of_range("subview out of range");
}

inline std::vector<uint8_t> ByteView::toVector() const {
    return readAsVector(0, size_);
}

template <class T>
inline T ByteView::get(size_t start) const {
    T value;
    if (start <= size_ && sizeof(T) <= size_ - start)
        memcpy(&value, data_ + start, sizeof(T));
    else
        throw std::out_of_range("read out of range");
    
    return value;
}

template <class T>
inline void ByteView::get(size_t start, T& dst) const {
    dst = get<T>(start);
}

inline void ByteView::read(size_t start, uint8_t* dst, size_t len) const {
    if (start > size_ || len > (size_ - start))
        throw std::out_of_range("read out of range");

    std::memcpy(dst, data_ + start, len);
}

inline std::vector<uint8_t> ByteView::readAsVector(size_t start, size_t len) const {
    if (start > size_ || len > (size_ - start))
        throw std::out_of_range("read out of range");

    std::vector<uint8_t> bytes(len);
    std::memcpy(bytes.data(), data_ + start, len);
    return bytes;
}

inline bool operator==(ByteView lhs, ByteView rhs) {
    return lhs.size() == rhs.size() &&
           (lhs.data() == rhs.data() || std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
}

inline bool operator!=(ByteView lhs, ByteView rhs) {
    return !(lhs == rhs);
}

} // namespace LiliumDB

#endif