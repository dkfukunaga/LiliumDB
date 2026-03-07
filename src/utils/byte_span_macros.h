#ifndef LILIUMDB_BYTE_SPAN_MACROS_H
#define LILIUMDB_BYTE_SPAN_MACROS_H

#include "byte_span.h"

// Helper macros

#define SPAN_WRITE_STRUCT_FIELD(span, struc, field)                        \
    (span).put<decltype(std::decay_t<decltype(struc)>::field)>(            \
        offsetof(std::decay_t<decltype(struc)>, field), (struc).field)

#define VIEW_READ_STRUCT_FIELD(view, struc, field)                 \
    (view).get<decltype(std::decay_t<decltype(struc)>::field)>(    \
        offsetof(std::decay_t<decltype(struc)>, field))

#endif