#ifndef LILIUMDB_RESULT_H
#define LILIUMDB_RESULT_H

#include "status.h"

#include <variant>
#include <utility>
#include <type_traits>
#include <cassert>

namespace LiliumDB {

// Ok and Err are tag types that wrap construction arguments to make the
// success and error cases of Result explicit at the call site.

// Ok wraps a value of type T for constructing a successful Result.
template <class T>
struct Ok {
    T val;
    template <class U>
    explicit Ok(U&& value) : val(std::forward<U>(value)) {}
};
template <class U>
Ok(U&&) -> Ok<std::decay_t<U>>;

// Err wraps a Status for constructing a failed Result.
// The Status must represent an error; asserts in debug builds if not.
struct Err {
    Status err;
    explicit Err(Status s) : err(std::move(s)) { assert(err.isErr()); }
};

// Represents either a successful value of type T or a Status error.
// Inspired by Rust's Result<T, E>.
//
// Always check isOk()/isErr() or test the bool conversion before accessing
// value() or error(). Incorrect access asserts in debug builds; in release,
// behavior depends on exception settings (throws std::bad_variant_access or
// terminates).
template<typename T>
class [[nodiscard]] Result {
public:
    static_assert(!std::is_same_v<T, Status>, "Result<Status> is not supported");

    // --- Constructors ---

    /// Constructs a successful Result wrapping the given value.
    Result(Ok<T> value): data_(std::in_place_type<T>, std::move(value.val)) { }
    /// Constructs a failed Result wrapping the given error Status.
    Result(Err error): data_(std::in_place_type<Status>, std::move(error.err)) { }

    // --- Observers ---

    /// Returns true if the Result holds a value.
    bool                isOk()  const noexcept { return std::holds_alternative<T>(data_); }
    /// Returns true if the Result holds an error.
    bool                isErr() const noexcept { return !isOk(); }
    /// True if the Result holds a value.
    explicit operator   bool()  const noexcept { return isOk(); }

    // --- Accessors ---

    /// Accesses the value. Asserts if the Result holds an error.
    T&                  value() &       { assert(isOk()); return std::get<T>(data_); }
    const T&            value() const & { assert(isOk()); return std::get<T>(data_); }
    T&&                 value() &&      { assert(isOk()); return std::get<T>(std::move(data_)); }

    /// Accesses the error Status. Asserts if the Result holds a value.
    Status&             error() &       { assert(!isOk()); return std::get<Status>(data_); }
    const Status&       error() const & { assert(!isOk()); return std::get<Status>(data_); }
    Status&&            error() &&      { assert(!isOk()); return std::get<Status>(std::move(data_)); }

    /// Member access and dereference operators. Assert if the Result holds an error.
    T*                  operator->()       { return &value(); }
    const T*            operator->() const { return &value(); }
    T&                  operator*()        { return value(); }
    const T&            operator*()  const { return value(); }

private:
    std::variant<T, Status> data_;
};

} // namespace LiliumDB

#endif