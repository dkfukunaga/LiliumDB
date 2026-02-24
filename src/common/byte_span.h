#ifndef LILIUMDB_BYTE_SPAN_H
#define LILIUMDB_BYTE_SPAN_H

#include <cstdint>      // uint8_t
#include <cstring>      // memcpy
#include <stdexcept>    // std::out_of_range
#include <iterator>

namespace LiliumDB {

class ByteView;

class ByteSpan {
public:
    ByteSpan(uint8_t* buf, size_t size):
        data_(buf),
        size_(size) { }

    uint8_t&                at(size_t index);
    ByteSpan                subspan(size_t start, size_t len) const;
    void                    write(size_t start, const uint8_t* src, size_t len);
    void                    write(size_t start, const ByteView& src);

    uint8_t*                data() const { return data_; }
    size_t                  size() const { return size_; }

    using iterator               = uint8_t*;
    using const_iterator         = const uint8_t*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator                begin()   { return data_; }
    iterator                end()     { return data_ + size_; }
    const_iterator          begin()   const { return data_; }
    const_iterator          end()     const { return data_ + size_; }
    const_iterator          cbegin()  const { return data_; }
    const_iterator          cend()    const { return data_ + size_; }

    reverse_iterator        rbegin()        { return reverse_iterator(end()); }
    reverse_iterator        rend()          { return reverse_iterator(begin()); }
    const_reverse_iterator  rbegin()  const { return const_reverse_iterator(end()); }
    const_reverse_iterator  rend()    const { return const_reverse_iterator(begin()); }
    const_reverse_iterator  crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator  crend()   const { return const_reverse_iterator(begin()); }
private:
    uint8_t*    data_;
    size_t      size_;
};

class ByteView {
public:
    ByteView(const uint8_t* buf, size_t size):
        data_(buf),
        size_(size) { }
    ByteView(const ByteSpan& span):
        data_(span.data()),
        size_(span.size()) { }

    uint8_t                 at(size_t index) const;
    ByteView                subview(size_t start, size_t len) const;
    void                    read(size_t start, uint8_t* dst, size_t size) const;

    const uint8_t*          data() const { return data_; }
    size_t                  size() const { return size_; }

    using const_iterator            = const uint8_t*;
    using const_reverse_iterator    = std::reverse_iterator<const_iterator>;

    const_iterator          begin()   const { return data_; }
    const_iterator          end()     const { return data_ + size_; }
    const_iterator          cbegin()  const { return data_; }
    const_iterator          cend()    const { return data_ + size_; }

    const_reverse_iterator  rbegin()  const { return const_reverse_iterator(end()); }
    const_reverse_iterator  rend()    const { return const_reverse_iterator(begin()); }
    const_reverse_iterator  crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator  crend()   const { return const_reverse_iterator(begin()); }
private:
    const uint8_t*  data_;
    size_t          size_;
};

// ByteSpan method definitions

inline uint8_t& ByteSpan::at(size_t index) {
    if (index < size_)
        return data_[index];
    else
        throw std::out_of_range("index out of range");
}

inline ByteSpan ByteSpan::subspan(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteSpan(data_ + start, len);
    else
        throw std::out_of_range("subspan out of range");
}

inline void ByteSpan::write(size_t start, const uint8_t* src, size_t len) {
    if (start <= size_ && len <= size_ - start)
        memcpy(data_ + start, src, len);
    else
        throw std::out_of_range("source out of range");
}

inline void ByteSpan::write(size_t start, const ByteView& src) {
    write(start, src.data(), src.size());
}

// ByteView method defintions

inline uint8_t ByteView::at(size_t index) const {
    if (index < size_)
        return data_[index];
    else
        throw std::out_of_range("index out of range");
}

inline ByteView ByteView::subview(size_t start, size_t len) const {
    if (start <= size_ && len <= size_ - start)
        return ByteView(data_ + start, len);
    else
        throw std::out_of_range("subview out of range");
}

inline void ByteView::read(size_t start, uint8_t* dst, size_t size) const {
    if (start <= size_ && size <= size_ - start)
        memcpy(dst, data_ + start, size);
    else
        throw std::out_of_range("size out of range");
}

} // namespace LiliumDB


#endif