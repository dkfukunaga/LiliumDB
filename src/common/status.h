#ifndef LILIUMDB_STATUS_H
#define LILIUMDB_STATUS_H

#include <string>
#include <string_view>
#include <utility>

namespace LiliumDB {

class Status {
public:
    enum class Code {
        Ok,
        FileErr,
        IOErr,
        Corrupt,
        Error
    };

    static Status       ok() noexcept { return Status(Code::Ok); }
    static Status       fileErr(std::string_view msg) { return Status(Code::FileErr, std::string(msg)); }
    static Status       ioErr(std::string_view msg) { return Status(Code::IOErr, std::string(msg)); }
    static Status       corrupt(std::string_view msg) { return Status(Code::Corrupt, std::string(msg)); }
    static Status       error(std::string_view msg) { return Status(Code::Error, std::string(msg)); }

    Code                code() const noexcept { return code_; }
    const std::string&  message() const noexcept { return message_; }

    bool                isOk() const noexcept { return code_ == Code::Ok; }
    bool                isError() const noexcept { return !isOk(); }
    bool                is(Code c) const noexcept { return code_ == c; }
private:
    explicit Status(Code c) noexcept: code_(c) {}
    Status(Code c, std::string msg): code_(c), message_(std::move(msg)) {}

    Code code_;
    std::string message_;
};

} // namespace LiliumDB

#endif