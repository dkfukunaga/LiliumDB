#ifndef LILIUMDB_HEXDUMP_H
#define LILIUMDB_HEXDUMP_H

#include "byte_span.h"

#include <ostream>
#include <cstdint>
#include <string_view>

void hexdump(
    std::ostream& out,
    const ByteView& view,
    uint64_t baseAddress = 0,
    std::string_view label = ""
);

#endif // LILIUMDB_HEXDUMP_H