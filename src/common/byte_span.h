#ifndef LILIUMDB_BYTE_SPAN_H
#define LILIUMDB_BYTE_SPAN_H

#include <cstdint>      // uint8_t
#include <cstddef>      // size_t, offsetof
#include <cstring>      // memcpy, memmove
#include <stdexcept>    // std::out_of_range
#include <cassert>      // assert
#include <iterator>     // std::reverse_iterator

namespace LiliumDB {

class ByteView;

class ByteSpan {
public:
    ByteSpan(): data_(nullptr), size_(0) { }
    ByteSpan(uint8_t* bytes, size_t size): data_(bytes), size_(size) {
        assert(data_ != nullptr || size_ == 0);
    }

    uint8_t&                        at(size_t offset);
    [[nodiscard]] ByteSpan          subspan(size_t start, size_t len);
    [[nodiscard]] ByteView          subview(size_t start, size_t len) const;

    void                            write(size_t start, const uint8_t* src, size_t len);
    void                            write(size_t start, const ByteView& src);
    template <typename T> void      write(size_t start, const T value);
    void                            copy_within(size_t to, size_t from, size_t len);

    uint8_t*                        data() noexcept { return data_; }
    const uint8_t*                  data() const noexcept { return data_; }
    size_t                          size() const noexcept { return size_; }

    uint8_t&                        operator[](size_t index) { return data_[index]; }
    const uint8_t&                  operator[](size_t index) const { return data_[index]; }

    using iterator                  = uint8_t*;
    using const_iterator            = const uint8_t*;
    using reverse_iterator          = std::reverse_iterator<iterator>;
    using const_reverse_iterator    = std::reverse_iterator<const_iterator>;

    iterator                        begin()         { return data_; }
    iterator                        end()           { return data_ + size_; }
    const_iterator                  begin()   const { return data_; }
    const_iterator                  end()     const { return data_ + size_; }
    const_iterator                  cbegin()  const { return data_; }
    const_iterator                  cend()    const { return data_ + size_; }

    reverse_iterator                rbegin()        { return reverse_iterator(end()); }
    reverse_iterator                rend()          { return reverse_iterator(begin()); }
    const_reverse_iterator          rbegin()  const { return const_reverse_iterator(end()); }
    const_reverse_iterator          rend()    const { return const_reverse_iterator(begin()); }
    const_reverse_iterator          crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator          crend()   const { return const_reverse_iterator(begin()); }
private:
    uint8_t*                        data_;
    size_t                          size_;
};

class ByteView {
public:
    ByteView(): data_(nullptr), size_(0) { }
    ByteView(const uint8_t* bytes, size_t size): data_(bytes), size_(size) {
        assert(data_ != nullptr || size_ == 0);
    }
    ByteView(const ByteSpan& span): data_(span.data()), size_(span.size()) { }

    uint8_t                         at(size_t offset) const;
    [[nodiscard]] ByteView          subview(size_t start, size_t len) const;

    void                            read(size_t start, uint8_t* dst, size_t len) const;
    template <typename T> T         read(size_t start) const;

    const uint8_t*                  data() const noexcept { return data_; }
    size_t                          size() const noexcept { return size_; }

    const uint8_t&                  operator[](size_t index) const { return data_[index]; }

    using const_iterator            = const uint8_t*;
    using const_reverse_iterator    = std::reverse_iterator<const_iterator>;

    const_iterator                  begin()   const { return data_; }
    const_iterator                  end()     const { return data_ + size_; }
    const_iterator                  cbegin()  const { return data_; }
    const_iterator                  cend()    const { return data_ + size_; }

    const_reverse_iterator          rbegin()  const { return const_reverse_iterator(end()); }
    const_reverse_iterator          rend()    const { return const_reverse_iterator(begin()); }
    const_reverse_iterator          crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator          crend()   const { return const_reverse_iterator(begin()); }
private:
    const uint8_t*                  data_;
    size_t                          size_;
};

// ByteSpan method definitions

inline uint8_t& ByteSpan::at(size_t offset) {
    if (offset < size_)
        return data_[offset];
    else
        throw std::out_of_range("offset out of range");
}

inline ByteSpan ByteSpan::subspan(size_t start, size_t len) {
    if (start <= size_ && len <= size_ - start)
        return ByteSpan(data_ + start, len);
    else
        throw std::out_of_range("subspan out of range");
}

inline ByteView ByteSpan::subview(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteView(data_ + start, len);
    else
        throw std::out_of_range("subview out of range");
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

template <typename T>
inline void ByteSpan::write(size_t start, const T value) {
    if (start <= size_ && sizeof(T) <= size_ - start)
        std::memcpy(data_ + start, &value, sizeof(T));
    else
        throw std::out_of_range("destination out of range");
}

inline void ByteSpan::copy_within(size_t to, size_t from, size_t len) {
    if (to <= size_ && from <= size_ && len <= size_ - to && len <= size_ - from)
        std::memmove(data_ + to, data_ + from, len);
    else
        throw std::out_of_range("move out of range");
}

// ByteView method definitions

inline uint8_t ByteView::at(size_t offset) const {
    if (offset < size_)
        return data_[offset];
    else
        throw std::out_of_range("offset out of range");
}

inline ByteView ByteView::subview(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteView(data_ + start, len);
    else
        throw std::out_of_range("subview out of range");
}

inline void ByteView::read(size_t start, uint8_t* dst, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        std::memcpy(dst, data_ + start, len);
    else
        throw std::out_of_range("read out of range");
}

template <typename T>
inline T ByteView::read(size_t start) const {
    T value;
    if (start <= size_ && sizeof(T) <= size_ - start)
        memcpy(&value, data_ + start, sizeof(T));
    else
        throw std::out_of_range("read out of range");
    
    return value;
}

// Helper macros

#define SPAN_WRITE_FIELD(span, header, field)                               \
    (span).write<decltype(std::decay_t<decltype(header)>::field)>(          \
        offsetof(std::decay_t<decltype(header)>, field), (header).field)

#define VIEW_READ_FIELD(view, header, field)                        \
    (view).read<decltype(std::decay_t<decltype(header)>::field)>(   \
        offsetof(std::decay_t<decltype(header)>, field))

} // namespace LiliumDB

#endif