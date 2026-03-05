#include "common/result.h"

#include <gtest/gtest.h>

using namespace LiliumDB;

TEST(ResultTest, Basic) {
    auto r1 = Result<int>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_FALSE(r1.isErr());
    EXPECT_EQ(r1.value(), 42);

    auto r2 = Result<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_FALSE(r2.isOk());
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error().code(), Status::Code::FileErr);
    EXPECT_EQ(r2.error().message(), "file not found");
}

TEST(ResultTest, BoolConversion) {
    auto r1 = Result<int>(Ok(42));
    EXPECT_TRUE(r1);

    auto r2 = Result<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_FALSE(r2);
}

TEST(ResultTest, ConstAccessors) {
    const auto r1 = Result<int>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), 42);

    const auto r2 = Result<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error().code(), Status::Code::FileErr);
    EXPECT_EQ(r2.error().message(), "file not found");
}

TEST(ResultTest, MoveOnly) {
    auto r1 = Result<std::unique_ptr<int>>(Ok(std::make_unique<int>(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(*r1.value(), 42);

    // Test rvalue accessor — move value out
    auto val = std::move(r1).value();
    EXPECT_EQ(*val, 42);

    auto r2 = Result<std::unique_ptr<int>>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());

    // Test rvalue accessor — move error out
    auto err = std::move(r2).error();
    EXPECT_EQ(err.code(), Status::Code::FileErr);
    EXPECT_EQ(err.message(), "file not found");
}

TEST(ResultTest, RvalueAccessors) {
    // value() &&
    auto val = Result<int>(Ok(42)).value();
    EXPECT_EQ(val, 42);

    // error() &&
    auto err = Result<int>(Err(Status::fileErr("file not found"))).error();
    EXPECT_EQ(err.code(), Status::Code::FileErr);
    EXPECT_EQ(err.message(), "file not found");
}

TEST(ResultTest, AccessorsAssert) {
    auto r1 = Result<int>(Ok(42));
    EXPECT_DEATH(r1.error(), ".*");
    r1.value(); // should not assert

    auto r2 = Result<int>(Err(Status::fileErr("file not found")));
    EXPECT_DEATH(r2.value(), ".*");
    r2.error(); // should not assert
}

TEST(ResultTest, ErrConstructorAsserts) {
    // Constructing Err with a non-error Status should assert
    EXPECT_DEATH(Result<int>(Err(Status::ok())), ".*");
}

TEST(ResultTest, Operators) {
    class TestClass {
    public:
        TestClass(int v): val_(v) { }
        operator int() const { return val_; }
        int getVal() const { return val_; }
    private:
        int val_;
    };

    auto r1 = Result<TestClass>(Ok(TestClass(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1->getVal(), 42);
    EXPECT_EQ((*r1).getVal(), 42);

    // const operator-> and operator*
    const auto& cr1 = r1;
    EXPECT_EQ(cr1->getVal(), 42);
    EXPECT_EQ((*cr1).getVal(), 42);

    auto r2 = Result<TestClass>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());
    EXPECT_DEATH(r2.operator->(), ".*");
    EXPECT_DEATH(*r2, ".*");
}

TEST(ResultTest, ReturnErgonomics) {
    auto okFn = []() -> Result<int> {
        return Ok(42);
    };

    auto errFn = []() -> Result<int> {
        return Err(Status::fileErr("file not found"));
    };

    auto ifFn = [](bool success) -> Result<int> {
        if (success) {
            return Ok(42);
        }
        return Err(Status::fileErr("file not found"));
    };

    EXPECT_TRUE(okFn());
    EXPECT_EQ(okFn().value(), 42);

    EXPECT_FALSE(errFn());
    EXPECT_EQ(errFn().error().code(), Status::Code::FileErr);

    EXPECT_TRUE(ifFn(true));
    EXPECT_EQ(ifFn(true).value(), 42);
    EXPECT_FALSE(ifFn(false));
    EXPECT_EQ(ifFn(false).error().code(), Status::Code::FileErr);
}