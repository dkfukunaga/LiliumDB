#ifndef LILIUMDB_RESULT_H
#define LILIUMDB_RESULT_H

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

// Err wraps an error of type E for constructing a failed Result.
template <class E>
struct Err {
    E err;
    template <class U>
    explicit Err(U&& error) : err(std::forward<U>(error)) { }
};
template <class U>
Err(U&&) -> Err<std::decay_t<U>>;

// Represents either a successful value of type T or an error of type E.
// Inspired by Rust's Result<T, E>.
//
// Always check isOk()/isErr() or test the bool conversion before accessing
// value() or error(). Incorrect access asserts in debug builds; in release,
// behavior depends on exception settings (throws std::bad_variant_access or
// terminates).
template<class T, class E>
class [[nodiscard]] Result {
public:
    static_assert(!std::is_same_v<T, E>, "Result<T, T> is not supported");

    // --- Constructors ---

    /// Constructs a successful Result wrapping the given value.
    Result(Ok<T> value): data_(std::in_place_type<T>, std::move(value.val)) { }
    /// Constructs a failed Result wrapping the given error.
    Result(Err<E> error): data_(std::in_place_type<E>, std::move(error.err)) { }

    // --- Observers ---

    /// Returns true if the Result holds a value.
    bool                isOk()  const noexcept { return std::holds_alternative<T>(data_); }
    /// Returns true if the Result holds an error.
    bool                isErr() const noexcept { return std::holds_alternative<E>(data_); }
    /// True if the Result holds a value, false if Result holds an error.
    explicit operator   bool()  const noexcept { return isOk(); }

    // --- Accessors ---

    /// Accesses the value. Asserts if the Result holds an error.
    T&                  value() &       { assert(isOk()); return std::get<T>(data_); }
    const T&            value() const & { assert(isOk()); return std::get<T>(data_); }
    T&&                 value() &&      { assert(isOk()); return std::get<T>(std::move(data_)); }

    /// Accesses the error. Asserts if the Result holds a value.
    E&                  error() &       { assert(!isOk()); return std::get<E>(data_); }
    const E&            error() const & { assert(!isOk()); return std::get<E>(data_); }
    E&&                 error() &&      { assert(!isOk()); return std::get<E>(std::move(data_)); }

    /// Member access and dereference operators. Assert if the Result holds an error.
    T*                  operator->() &       { return &value(); }
    const T*            operator->() const & { return &value(); }
    T&                  operator*()  &       { return value(); }
    const T&            operator*()  const & { return value(); }
    T&&                 operator*()  &&      { return value(); }

private:
    std::variant<T, E> data_;
};

} // namespace LiliumDB

#endif