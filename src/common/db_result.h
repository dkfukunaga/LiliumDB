#ifndef LILIUMDB_DB_RESULT_H
#define LILIUMDB_DB_RESULT_H

#include "utils/result.h"
#include "utils/result_macros.h"
#include "status.h"

namespace LiliumDB {

/// Convenience alias for Result<T, Status>.
template<class T>
using DbResult = Result<T, Status>;

} // namespace LiliumDB

#endif