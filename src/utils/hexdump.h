#ifndef LILIUMDB_HEXDUMP_H
#define LILIUMDB_HEXDUMP_H

#include "byte_span.h"

#include <ostream>
#include <cstdint>
#include <string_view>

namespace LiliumDB {

void hexdump(
    std::ostream& out,
    const ByteView& view,
    uint64_t base_address = 0,
    std::string_view label = ""
);

} // namespace LiliumDB

#endif // LILIUMDB_HEXDUMP_H