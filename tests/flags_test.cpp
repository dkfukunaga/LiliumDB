#include "utils/flags.h"

#include "gtest/gtest.h"

enum class TestFlag : uint16_t {
    A = 0x01,
    B = 0x02,
    C = 0x04,
    D = 0x08,
};

TEST(FlagsTest, BasicOperations) {
    Flags<TestFlag> flags;

    EXPECT_FALSE(flags);
    EXPECT_FALSE(flags.hasAny({}));
    EXPECT_TRUE(flags.hasAll({}));
    EXPECT_FALSE(flags.hasAny({TestFlag::A, TestFlag::B}));
    EXPECT_FALSE(flags.hasAll({TestFlag::A, TestFlag::B}));

    flags.set(TestFlag::A);
    EXPECT_TRUE(flags.has(TestFlag::A));
    EXPECT_FALSE(flags.has(TestFlag::B));
    EXPECT_TRUE(flags.hasAny({TestFlag::A, TestFlag::B}));
    EXPECT_TRUE(flags.hasAll({TestFlag::A}));
    EXPECT_FALSE(flags.hasAll({TestFlag::A, TestFlag::B}));

    flags.set(TestFlag::B);
    EXPECT_TRUE(flags.has(TestFlag::A));
    EXPECT_TRUE(flags.has(TestFlag::B));
    EXPECT_TRUE(flags.hasAny({TestFlag::A, TestFlag::B}));
    EXPECT_TRUE(flags.hasAll({TestFlag::A, TestFlag::B}));

    flags.remove(TestFlag::A);
    EXPECT_FALSE(flags.has(TestFlag::A));
    EXPECT_TRUE(flags.has(TestFlag::B));
}

TEST(FlagsTest, ToggleAndClear) {
    Flags<TestFlag> flags({TestFlag::A, TestFlag::C});

    flags.toggle(TestFlag::B);
    EXPECT_TRUE(flags.has(TestFlag::B));

    flags.toggle(TestFlag::A);
    EXPECT_FALSE(flags.has(TestFlag::A));

    flags.clear();
    EXPECT_FALSE(flags);
}

TEST(FlagsTest, BitwiseOperators) {
    Flags<TestFlag> flags1({TestFlag::A, TestFlag::B});
    Flags<TestFlag> flags2({TestFlag::B, TestFlag::C});

    auto flagsOr = flags1 | flags2;
    EXPECT_TRUE(flagsOr.has(TestFlag::A));
    EXPECT_TRUE(flagsOr.has(TestFlag::B));
    EXPECT_TRUE(flagsOr.has(TestFlag::C));

    auto flagsAnd = flags1 & flags2;
    EXPECT_FALSE(flagsAnd.has(TestFlag::A));
    EXPECT_TRUE(flagsAnd.has(TestFlag::B));
    EXPECT_FALSE(flagsAnd.has(TestFlag::C));

    auto flagsXor = flags1 ^ flags2;
    EXPECT_TRUE(flagsXor.has(TestFlag::A));
    EXPECT_FALSE(flagsXor.has(TestFlag::B));
    EXPECT_TRUE(flagsXor.has(TestFlag::C));

    auto flagsNot = ~flags1;
    EXPECT_FALSE(flagsNot.has(TestFlag::A));
    EXPECT_FALSE(flagsNot.has(TestFlag::B));
    EXPECT_TRUE(flagsNot.has(TestFlag::C));
}

TEST(FlagsTest, ComparisonOperators) {
    Flags<TestFlag> flags1({TestFlag::A, TestFlag::B});
    Flags<TestFlag> flags2({TestFlag::A, TestFlag::B});
    Flags<TestFlag> flags3({TestFlag::A, TestFlag::C});

    EXPECT_TRUE(flags1 == flags2);
    EXPECT_FALSE(flags1 == flags3);
    EXPECT_TRUE(flags1 != flags3);
}

TEST(FlagsTest, BitsToString) {
    Flags<TestFlag> flags({TestFlag::A, TestFlag::C});
    EXPECT_EQ(flags.toBitString(), "0000 0000 0000 0101");

    flags.set(TestFlag::D);
    EXPECT_EQ(flags.toBitString(), "0000 0000 0000 1101");
}