#ifndef RISA_IO_H_GUARD
#define RISA_IO_H_GUARD

#include "../api.h"
#include "../def/types.h"

#include <stdarg.h>

typedef void (*RisaOutHandler)(const char*);
typedef uint16_t (*RisaInHandler)();

typedef struct {
    RisaInHandler  in;
    RisaOutHandler out;
    RisaOutHandler err;
} RisaIO;

RISA_API void risa_io_init(RisaIO* io);

RISA_API void risa_io_redirect_in(RisaIO* io, RisaInHandler handler);
RISA_API void risa_io_redirect_out(RisaIO* io, RisaOutHandler handler);
RISA_API void risa_io_redirect_err(RisaIO* io, RisaOutHandler handler);

RISA_API void risa_io_clone(RisaIO* dest, RisaIO* src);

// Uppercase because they are the default functions for IO.
RISA_API uint16_t RISA_IO_STDIN();
RISA_API void     RISA_IO_STDOUT(const char* data);
RISA_API void     RISA_IO_STDERR(const char* data);

RISA_API uint16_t risa_io_in(RisaIO* io);
RISA_API void     risa_io_out(RisaIO* io, const char* fmt, ...);
RISA_API void     risa_io_err(RisaIO* io, const char* fmt, ...);

RISA_API char* risa_io_format(const char* fmt, va_list list);

#endif
