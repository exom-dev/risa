#ifndef RISA_LOG_H_GUARD
#define RISA_LOG_H_GUARD

#include "io.h"
#include "../def/def.h"

#include <stdio.h>
#include <stdlib.h>

#define RISA_IN(io) \
    risa_io_in(&io)

#define RISA_OUT(io, fmt, ...) \
    risa_io_out(&io, fmt, ##__VA_ARGS__)

#define RISA_ERR(io, fmt, ...) \
    risa_io_err(&io, fmt, ##__VA_ARGS__)

#define RISA_WARNING(io, fmt, ...) \
    RISA_OUT(io, "[warning] " fmt "\n", ##__VA_ARGS__)

#define RISA_ERROR(io, fmt, ...) \
    RISA_ERR(io, "[error] " fmt "\n", ##__VA_ARGS__)

#ifdef DEBUG_TRACE_MEMORY_OPS
    #define RISA_MEMORY(fmt, ...) \
        fprintf(stdout, "[memory] " fmt "\n", ##__VA_ARGS__)
#else
    #define RISA_MEMORY(fmt, ...)
#endif

#endif
