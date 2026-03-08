#ifndef LILIUMDB_RESULT_MACROS_H
#define LILIUMDB_RESULT_MACROS_H

#include "result.h"

// --- Error propagation macros

// Evaluates expr (which must return a Result<T, E> or Result<void, E>).
// On error, returns the error to the caller.
#define RETURN_ON_ERROR(expr)                       \
    do {                                            \
        auto&& _r = (expr);                         \
        if (!_r) return Err(std::move(_r).error()); \
    } while (0)

// Evaluates expr (which must return a Result<T, E>).
// On success, assigns the unwrapped value to var.
// On error, returns the error to the caller.
#define ASSIGN_OR_RETURN(var, expr)                 \
    do {                                            \
        auto&& _r = (expr);                         \
        if (!_r) return Err(std::move(_r).error()); \
        (var) = std::move(_r).value();              \
    } while (0)

#endif