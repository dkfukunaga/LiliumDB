#ifndef LILIUMDB_MACROS_H
#define LILIUMDB_MACROS_H

#include "status.h"
#include "result.h"

namespace LiliumDB {

// --- Error propagation macros

// Evaluates expr (which must return a Status) and propagates the Status
// error to the caller on failure.
#define TRY_STATUS(expr)                    \
    do {                                    \
        auto _s = (expr);                   \
        if (_s.isErr()) return _s.error();  \
    } while(0)

// Evaluates expr (which must return a Result<T>), and either forwards
// the wrapped value or propagates the wrapped error to the caller.
#define TRY_RESULT_FWD(expr)                    \
    do {                                        \
        auto _r = (expr);                       \
        if (_r.isErr()) return Err(_r.error()); \
        return Ok(_r.value());                  \
    } while (0)

// Evaluates expr (which must return a Result<T>), and either binds the
// unwrapped value to var or propagates the wrapped error to the caller.
#define TRY_RESULT_BIND_IMPL(expr, var, N)          \
    auto _r_##N = (expr);                           \
    if (_r_##N.isErr()) return Err(_r_##N.error()); \
    auto var = std::move(_r_##N.value())

#define TRY_RESULT_BIND(expr, var) TRY_RESULT_BIND_IMPL(expr, var, __COUNTER__)

// Evaluates expr (which must return a Result<T>), and either binds the
// unwrapped value to var or propagates the Status error to the caller.
#define TRY_STATUS_BIND_IMPL(expr, var, N)      \
    auto _r_##N = (expr);                       \
    if (_r_##N.isErr()) return _r_##N.error();  \
    auto var = std::move(_r_##N.value())

#define TRY_STATUS_BIND(expr, var) TRY_STATUS_BIND_IMPL(expr, var, __COUNTER__)

} // namespace LiliumDB

#endif