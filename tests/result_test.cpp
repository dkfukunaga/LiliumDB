#include "utils/result.h"
#include "utils/result_macros.h"

#include <gtest/gtest.h>

using namespace LiliumDB;

TEST(ResultTest, Basic) {
    auto r1 = Result<int, std::string>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_FALSE(r1.isErr());
    EXPECT_EQ(r1.value(), 42);

    auto r2 = Result<std::string, int>(Err(404));
    EXPECT_FALSE(r2.isOk());
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error(), 404);
}

TEST(ResultTest, BoolConversion) {
    auto r1 = Result<int, std::string>(Ok(42));
    EXPECT_TRUE(r1);

    auto r2 = Result<std::string, int>(Err(404));
    EXPECT_FALSE(r2);
}

TEST(ResultTest, ConstAccessors) {
    const auto r1 = Result<int, std::string>(Ok(42));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), 42);

    const auto r2 = Result<std::string, int>(Err(404));
    EXPECT_TRUE(r2.isErr());
    EXPECT_EQ(r2.error(), 404);
}

TEST(ResultTest, MoveOnly) {
    auto r1 = Result<std::unique_ptr<int>, std::string>(Ok(std::make_unique<int>(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(*r1.value(), 42);

    // Test rvalue accessor — move value out
    auto val = std::move(r1).value();
    EXPECT_EQ(*val, 42);

    auto r2 = Result<std::string, std::unique_ptr<int>>(Err(std::make_unique<int>(404)));
    EXPECT_TRUE(r2.isErr());

    // Test rvalue accessor — move error out
    auto err = std::move(r2).error();
    EXPECT_EQ(*err, 404);
}

TEST(ResultTest, RvalueAccessors) {
    // value() &&
    auto val = Result<int, std::string>(Ok(42)).value();
    EXPECT_EQ(val, 42);

    // error() &&
    auto err = Result<std::string, int>(Err(404)).error();
    EXPECT_EQ(err, 404);
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

    auto r1 = Result<TestClass, std::string>(Ok(TestClass(42)));
    EXPECT_TRUE(r1.isOk());
    EXPECT_EQ(r1->getVal(), 42);
    EXPECT_EQ((*r1).getVal(), 42);

    // const operator-> and operator*
    const auto& cr1 = r1;
    EXPECT_EQ(cr1->getVal(), 42);
    EXPECT_EQ((*cr1).getVal(), 42);

    auto r2 = Result<std::string, TestClass>(Err(TestClass(42)));
    EXPECT_TRUE(r2.isErr());
    EXPECT_DEATH(r2.operator->(), ".*");
    EXPECT_DEATH(*r2, ".*");
}

TEST(ResultTest, AccessorsAssert) {
    auto r1 = Result<int, std::string>(Ok(42));
    EXPECT_DEATH(r1.error(), ".*");

    auto r2 = Result<int, std::string>(Err(std::string("error")));
    EXPECT_DEATH(r2.value(), ".*");
}

TEST(ResultTest, ReturnErgonomics) {
    auto okFn = []() -> Result<int, std::string> {
        return Ok(42);
    };

    auto errFn = []() -> Result<int, std::string> {
        return Err(std::string("error"));
    };

    EXPECT_TRUE(okFn());
    EXPECT_EQ(okFn().value(), 42);
    EXPECT_FALSE(errFn());
    EXPECT_EQ(errFn().error(), "error");
}

TEST(ResultTest, Macros) {
    auto r1 = Result<int, std::string>(Ok(42));
    EXPECT_TRUE(r1);
    EXPECT_EQ(r1.value(), 42);

    auto r2 = Result<std::string, int>(Err(404));
    EXPECT_FALSE(r2);
    EXPECT_EQ(r2.error(), 404);
}