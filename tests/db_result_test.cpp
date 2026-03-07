#include "common/status.h"

#include <gtest/gtest.h>

using namespace LiliumDB;

TEST(DbResultTest, Basic) {
    auto r1 = DbResult<int>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_FALSE(r1.isErr());
    EXPECT_EQ(r1.value(), 42);

    auto r2 = DbResult<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_FALSE(r2.isOk());
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error().code(), Status::Code::FileErr);
    EXPECT_EQ(r2.error().message(), "file not found");
}

TEST(DbResultTest, BoolConversion) {
    auto r1 = DbResult<int>(Ok(42));
    EXPECT_TRUE(r1);

    auto r2 = DbResult<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_FALSE(r2);
}

TEST(DbResultTest, ConstAccessors) {
    const auto r1 = DbResult<int>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), 42);

    const auto r2 = DbResult<std::string>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error().code(), Status::Code::FileErr);
    EXPECT_EQ(r2.error().message(), "file not found");
}

TEST(DbResultTest, MoveOnly) {
    auto r1 = DbResult<std::unique_ptr<int>>(Ok(std::make_unique<int>(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(*r1.value(), 42);

    // Test rvalue accessor — move value out
    auto val = std::move(r1).value();
    EXPECT_EQ(*val, 42);

    auto r2 = DbResult<std::unique_ptr<int>>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());

    // Test rvalue accessor — move error out
    auto err = std::move(r2).error();
    EXPECT_EQ(err.code(), Status::Code::FileErr);
    EXPECT_EQ(err.message(), "file not found");
}

TEST(DbResultTest, RvalueAccessors) {
    // value() &&
    auto val = DbResult<int>(Ok(42)).value();
    EXPECT_EQ(val, 42);

    // error() &&
    auto err = DbResult<int>(Err(Status::fileErr("file not found"))).error();
    EXPECT_EQ(err.code(), Status::Code::FileErr);
    EXPECT_EQ(err.message(), "file not found");
}

TEST(DbResultTest, AccessorsAssert) {
    auto r1 = DbResult<int>(Ok(42));
    EXPECT_DEATH(r1.error(), ".*");
    r1.value(); // should not assert

    auto r2 = DbResult<int>(Err(Status::fileErr("file not found")));
    EXPECT_DEATH(r2.value(), ".*");
    r2.error(); // should not assert
}

TEST(DbResultTest, Operators) {
    class TestClass {
    public:
        TestClass(int v): val_(v) { }
        operator int() const { return val_; }
        int getVal() const { return val_; }
    private:
        int val_;
    };

    auto r1 = DbResult<TestClass>(Ok(TestClass(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1->getVal(), 42);
    EXPECT_EQ((*r1).getVal(), 42);

    // const operator-> and operator*
    const auto& cr1 = r1;
    EXPECT_EQ(cr1->getVal(), 42);
    EXPECT_EQ((*cr1).getVal(), 42);

    auto r2 = DbResult<TestClass>(Err(Status::fileErr("file not found")));
    EXPECT_TRUE(r2.isErr());
    EXPECT_DEATH(r2.operator->(), ".*");
    EXPECT_DEATH(*r2, ".*");
}

TEST(DbResultTest, ReturnErgonomics) {
    auto okFn = []() -> DbResult<int> {
        return Ok(42);
    };

    auto errFn = []() -> DbResult<int> {
        return Err(Status::fileErr("file not found"));
    };

    auto ifFn = [](bool success) -> DbResult<int> {
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

TEST(DbResultTest, VoidResult) {
    auto okFn = []() -> VoidResult {
        return Ok(Success);
    };

    auto errFn = []() -> VoidResult {
        return Err(Status::fileErr("file not found"));
    };

    EXPECT_TRUE(okFn());
    EXPECT_FALSE(errFn());
    EXPECT_EQ(okFn().value(), Success);
    EXPECT_EQ(errFn().error().code(), Status::Code::FileErr);
}