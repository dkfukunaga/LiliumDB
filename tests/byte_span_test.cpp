#include "utils/byte_span.h"
#include "utils/byte_span_macros.h"

#include <vector>

#include <gtest/gtest.h>

using namespace LiliumDB;

TEST(ByteSpanTest, Basic) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    ByteSpan span(data.data(), data.size());

    EXPECT_EQ(span.size(), 5);
    EXPECT_EQ(span[0], 1);
    EXPECT_EQ(span[4], 5);

    span[2] = 42;
    EXPECT_EQ(span[2], 42);

    auto subspan = span.subspan(1, 3);
    EXPECT_EQ(subspan.size(), 3);
    EXPECT_EQ(subspan[0], 2);
    EXPECT_EQ(subspan[2], 4);

    auto subview = span.subview(0, 4);
    EXPECT_EQ(subview.size(), 4);
    EXPECT_EQ(subview[0], 1);
    EXPECT_EQ(subview[3], 4);
}

TEST(ByteSpanTest, WriteAndCopy) {
    std::vector<uint8_t> data = {0, 0, 0, 0, 0};
    ByteSpan span(data.data(), data.size());

    uint8_t src[] = {10, 20, 30};
    span.write(1, src, 3);
    EXPECT_EQ(span[1], 10);
    EXPECT_EQ(span[2], 20);
    EXPECT_EQ(span[3], 30);
    EXPECT_EQ(span[4], 0);

    span.copy_within(0, 2, 3);
    EXPECT_EQ(span[0], 20);
    EXPECT_EQ(span[1], 30);
    EXPECT_EQ(span[2], 0);
    EXPECT_EQ(span[3], 30);
    EXPECT_EQ(span[4], 0);
}

TEST(ByteSpanTest, OutOfRange) {
    std::vector<uint8_t> data = {1, 2, 3};
    ByteSpan span(data.data(), data.size());

    EXPECT_THROW((void)span.subspan(2, 2), std::out_of_range);
    EXPECT_THROW((void)span.subview(3, 1), std::out_of_range);
}

TEST(ByteSpanTest, Iterators) {
    std::vector<uint8_t> data = {10, 20, 30};
    ByteSpan span(data.data(), data.size());

    std::vector<uint8_t> collected(span.begin(), span.end());
    EXPECT_EQ(collected, data);

    std::vector<uint8_t> reverse_collected(span.rbegin(), span.rend());
    EXPECT_EQ(reverse_collected, std::vector<uint8_t>({30, 20, 10}));
}

TEST(ByteSpanTest, SetAndClear) {
    std::vector<uint8_t> data = {10, 10, 10, 10};
    ByteSpan span(data.data(), data.size());

    std::vector<uint8_t> zero(4);
    span.clear();
    std::vector<uint8_t> collected(span.begin(), span.end());
    EXPECT_EQ(collected, zero);

    std::vector<uint8_t> sixseven = {67, 67, 67, 67};
    span.set(67);
    collected = std::vector<uint8_t>(span.begin(), span.end());
    EXPECT_EQ(collected, sixseven);
}

TEST(ByteViewTest, Basic) {
    std::vector<uint8_t> data = {5, 10, 15, 20};
    ByteSpan span(data.data(), data.size());
    ByteView view(span);

    EXPECT_EQ(view.size(), 4);
    EXPECT_EQ(view[0], 5);
    EXPECT_EQ(view[3], 20);

    auto subview = view.subview(1, 2);
    EXPECT_EQ(subview.size(), 2);
    EXPECT_EQ(subview[0], 10);
    EXPECT_EQ(subview[1], 15);
}

TEST(ByteViewTest, OutOfRange) {
    std::vector<uint8_t> data = {1, 2, 3};
    ByteView view(data.data(), data.size());

    EXPECT_THROW((void)view.subview(2, 2), std::out_of_range);
    EXPECT_THROW((void)view.subview(3, 1), std::out_of_range);
}

TEST(ByteViewTest, Iterators) {
    std::vector<uint8_t> data = {100, 200, 255};
    ByteView view(data.data(), data.size());

    std::vector<uint8_t> collected(view.begin(), view.end());
    EXPECT_EQ(collected, data);

    std::vector<uint8_t> reverse_collected(view.rbegin(), view.rend());
    EXPECT_EQ(reverse_collected, std::vector<uint8_t>({255, 200, 100}));
}

TEST(ByteViewTest, GetAsVector) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    ByteView view(data.data(), data.size());

    auto vec = view.readAsVector(1, 3);
    EXPECT_EQ(vec, std::vector<uint8_t>({2, 3, 4}));
}

struct TestHeader {
    uint16_t id;
    uint32_t offset;
    uint8_t  flags;
};

TEST(ByteSpanMacrosTest, RoundTrip) {

    std::vector<uint8_t> buf(sizeof(TestHeader), 0);
    ByteSpan span(buf.data(), buf.size());
    ByteView view(buf.data(), buf.size());
    TestHeader header1{ 42, 1024, 0xFF };

    SPAN_WRITE_STRUCT_FIELD(span, header1, id);
    SPAN_WRITE_STRUCT_FIELD(span, header1, offset);
    SPAN_WRITE_STRUCT_FIELD(span, header1, flags);

    TestHeader header2{};

    VIEW_READ_STRUCT_FIELD(view, header2, id);
    VIEW_READ_STRUCT_FIELD(view, header2, offset);
    VIEW_READ_STRUCT_FIELD(view, header2, flags);

    EXPECT_EQ(header2.id, header1.id);
    EXPECT_EQ(header2.offset, header1.offset);
    EXPECT_EQ(header2.flags, header1.flags);
}

TEST(ByteSpanMacrosTest, CorrectOffset) {
    std::vector<uint8_t> buf(sizeof(TestHeader), 0);
    ByteSpan span(buf.data(), buf.size());
    TestHeader header{ 42, 1024, 0xFF };

    SPAN_WRITE_STRUCT_FIELD(span, header, offset);

    uint32_t raw;
    std::memcpy(&raw, buf.data() + offsetof(TestHeader, offset), sizeof(uint32_t));
    EXPECT_EQ(raw, header.offset);
}