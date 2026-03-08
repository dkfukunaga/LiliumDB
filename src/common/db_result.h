#ifndef LILIUMDB_DB_RESULT_H
#define LILIUMDB_DB_RESULT_H

#include "utils/result.h"
#include "status.h"

#include <variant>      // std::monostate

namespace LiliumDB {

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