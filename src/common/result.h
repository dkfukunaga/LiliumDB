#ifndef LILIUMDB_RESULT_H
#define LILIUMDB_RESULT_H

#include "status.h"

#include <variant>
#include <utility>
#include <type_traits>
#include <cassert>

namespace LiliumDB {

// Represents either a successful value of type T or a Status error.
// Inspired by Rust's Result<T, E>. Use isOk()/isErr() or if (result) before
// accessing value or error.
// Asserts in debug; in release, wrong access will throw
// std::bad_variant_access (or terminate if exceptions are disabled).
template<typename T>
class [[nodiscard]] Result {
public:
    // Prevent ambiguous instantiation
    static_assert(!std::is_same_v<T, Status>, "Result<Status> is not supported");

    // --- Construction ---

    /// Constructs a successful Result wrapping the given value.
    static Result       ok(T value) { return Result(std::move(value)); }
    /// Constructs a failed Result wrapping the given error Status.
    /// The Status must be an error; asserts in debug builds if not.
    static Result       err(Status status) { assert(status.isErr()); return Result(std::move(status)); }

    // --- Observers ---

    /// Returns true if the Result holds a value.
    bool                isOk()  const noexcept { return std::holds_alternative<T>(data_); }
    /// Returns true if the Result holds an error.
    bool                isErr() const noexcept { return !isOk(); }
    /// Implicit bool conversion; true if the Result holds a value.
    explicit operator   bool()  const noexcept { return isOk(); }

    // --- Accessors ---

    // Accessors (debug-assert on misuse)
    T&                  value() &       { assert(isOk()); return std::get<T>(data_); }
    const T&            value() const & { assert(isOk()); return std::get<T>(data_); }
    T&&                 value() &&      { assert(isOk()); return std::get<T>(std::move(data_)); }

    /// Accesses the error Status. Asserts in debug builds if the Result is a value.
    Status&             error() &       { assert(!isOk()); return std::get<Status>(data_); }
    const Status&       error() const & { assert(!isOk()); return std::get<Status>(data_); }
    Status&&            error() &&      { assert(!isOk()); return std::get<Status>(std::move(data_)); }

    /// Accesses the value directly. Asserts in debug builds if the Result is an error.
    T*                  operator->()       { return &value(); }
    const T*            operator->() const { return &value(); }
    T&                  operator*()        { return value(); }
    const T&            operator*()  const { return value(); }

private:
    explicit Result(T value): data_(std::move(value)) { }
    explicit Result(Status status): data_(std::move(status)) { }

    std::variant<T, Status> data_;
};

} // namespace LiliumDB

#endif