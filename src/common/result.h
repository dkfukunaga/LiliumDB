#ifndef LILIUMDB_RESULT_H
#define LILIUMDB_RESULT_H

#include "status.h"

#include <variant>
#include <utility>
#include <cassert>

namespace LiliumDB {

template<typename T>
class [[nodiscard]] Result {
public:
    // Prevent ambiguous instantiation
    static_assert(!std::is_same_v<T, Status>, "Result<Status> is not supported");

    // Named constructors
    static Result       ok(T value) { return Result(std::move(value)); }
    static Result       err(Status status) { assert(status.isError()); return Result(std::move(status)); }

    // Observers
    bool                isOk()  const noexcept { return std::holds_alternative<T>(data_); }
    bool                isErr() const noexcept { return !isOk(); }
    explicit operator   bool()  const noexcept { return isOk(); }

    // Operator overrides
    T*                  operator->()       { return &value(); }
    const T*            operator->() const { return &value(); }
    T&                  operator*()        { return value(); }
    const T&            operator*()  const { return value(); }

    // Accessors (debug-assert on misuse)
    T&                  value() &       { assert(isOk()); return std::get<T>(data_); }
    const T&            value() const & { assert(isOk()); return std::get<T>(data_); }
    T&&                 value() &&      { assert(isOk()); return std::get<T>(std::move(data_)); }
    Status&             error() &       { assert(!isOk()); return std::get<Status>(data_); }
    const Status&       error() const & { assert(!isOk()); return std::get<Status>(data_); }
    Status&&            error() &&      { assert(!isOk()); return std::get<Status>(std::move(data_)); }
private:
    explicit Result(T value): data_(std::move(value)) { }
    explicit Result(Status status): data_(std::move(status)) { }

    std::variant<T, Status> data_;
};

} // namespace LiliumDB

#endif