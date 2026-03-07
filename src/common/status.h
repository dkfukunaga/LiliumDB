#ifndef LILIUMDB_STATUS_H
#define LILIUMDB_STATUS_H

#include "utils/result.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace LiliumDB {

class Status {
public:
    enum class Code {
        Ok,
        FileErr,
        IOErr,
        FileInvalid,
        Corrupt,
        Error
    };

    static Status       ok() noexcept { return Status(Code::Ok); }
    static Status       fileErr(std::string_view msg) { return Status(Code::FileErr, std::string(msg)); }
    static Status       ioErr(std::string_view msg) { return Status(Code::IOErr, std::string(msg)); }
    static Status       fileInvalid(std::string_view msg) { return Status(Code::FileInvalid, std::string(msg)); }
    static Status       corrupt(std::string_view msg) { return Status(Code::Corrupt, std::string(msg)); }
    static Status       error(std::string_view msg) { return Status(Code::Error, std::string(msg)); }

    Code                code() const noexcept { return code_; }
    const std::string&  message() const noexcept { return message_; }

    bool                isOk() const noexcept { return code_ == Code::Ok; }
    bool                isErr() const noexcept { return !isOk(); }
    bool                is(Code c) const noexcept { return code_ == c; }
private:
    explicit Status(Code c) noexcept: code_(c) {}
    Status(Code c, std::string msg): code_(c), message_(std::move(msg)) {}

    Code code_;
    std::string message_;
};

/// Convenience alias for Result<T, Status>.
template<class T>
using DbResult = Result<T, Status>;

/// Convenience alias for Result<std::monostate, Status> for use with functions
/// with no return type.
/// Use: on a success, return Ok(Success).
using VoidResult = DbResult<std::monostate>;

/// Sentinel value for use with VoidResult.
inline constexpr std::monostate Success{};

} // namespace LiliumDB

#endif