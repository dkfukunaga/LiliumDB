#include "common/status.h"

#include <gtest/gtest.h>

using namespace LiliumDB;

TEST(StatusTest, Basic) {
    auto s1 = Status::ok();
    EXPECT_TRUE(s1.isSuccess());
    EXPECT_FALSE(s1.isError());
    EXPECT_TRUE(s1.is(Status::Code::Ok));
    EXPECT_EQ(s1.message(), "");

    auto s2 = Status::newFile();
    EXPECT_TRUE(s2.isSuccess());
    EXPECT_FALSE(s2.isError());
    EXPECT_TRUE(s2.is(Status::Code::NewFile));
    EXPECT_EQ(s2.message(), "");

    auto s3 = Status::fileErr("file not found");
    EXPECT_FALSE(s3.isSuccess());
    EXPECT_TRUE(s3.isError());
    EXPECT_TRUE(s3.is(Status::Code::FileErr));
    EXPECT_EQ(s3.message(), "file not found");

    auto s4 = Status::ioErr("disk error");
    EXPECT_FALSE(s4.isSuccess());
    EXPECT_TRUE(s4.isError());
    EXPECT_TRUE(s4.is(Status::Code::IOErr));
    EXPECT_EQ(s4.message(), "disk error");

    auto s5 = Status::corrupt("data corrupted");
    EXPECT_FALSE(s5.isSuccess());
    EXPECT_TRUE(s5.isError());
    EXPECT_TRUE(s5.is(Status::Code::Corrupt));
    EXPECT_EQ(s5.message(), "data corrupted");

    auto s6 = Status::error("unknown error");
    EXPECT_FALSE(s6.isSuccess());
    EXPECT_TRUE(s6.isError());
    EXPECT_TRUE(s6.is(Status::Code::Error));
    EXPECT_EQ(s6.message(), "unknown error");
}