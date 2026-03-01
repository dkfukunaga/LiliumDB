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
        NewFile,
        ErrFile,
        ErrIO,
        ErrCorrupt,
        Error
    };

    static Status       ok() noexcept { return Status(Code::Ok); }
    static Status       newFile(std::string_view msg) { return Status(Code::NewFile, std::string(msg)); }
    static Status       file(std::string_view msg) { return Status(Code::ErrFile, std::string(msg)); }
    static Status       io(std::string_view msg) { return Status(Code::ErrIO, std::string(msg)); }
    static Status       corrupt(std::string_view msg) { return Status(Code::ErrCorrupt, std::string(msg)); }
    static Status       error(std::string_view msg) { return Status(Code::Error, std::string(msg)); }

    Code                code() const noexcept { return code_; }
    const std::string&  message() const { return message_; }

    bool                isOk() const noexcept { return code_ == Code::Ok; }
    bool                is(Code c) const noexcept { return code_ == c; }
private:
    explicit Status(Code c) noexcept: code_(c) {}
    Status(Code c, std::string msg): code_(c), message_(std::move(msg)) {}

    Code code_;
    std::string message_;
};

} // namespace LiliumDB

#endif