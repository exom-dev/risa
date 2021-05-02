#ifndef RISA_IO_H_GUARD
#define RISA_IO_H_GUARD

#include "../api.h"
#include "../def/types.h"

#include <stdarg.h>

typedef enum {
    RISA_INPUT_MODE_CHAR = 0,
    RISA_INPUT_MODE_WORD = 1,
    RISA_INPUT_MODE_LINE = 2
} RisaInputMode;

typedef void (*RisaOutHandler)(const char*);
typedef char* (*RisaInHandler)(uint8_t);

typedef struct {
    RisaInHandler  in;
    RisaOutHandler out;
    RisaOutHandler err;
    bool freeInput;
} RisaIO;

RISA_API RisaIO*  risa_io_create ();
RISA_API void     risa_io_init   (RisaIO* io);
RISA_API void     risa_io_free   (RisaIO* io);

RISA_API void  risa_io_redirect_in    (RisaIO* io, RisaInHandler handler);
RISA_API void  risa_io_redirect_out   (RisaIO* io, RisaOutHandler handler);
RISA_API void  risa_io_redirect_err   (RisaIO* io, RisaOutHandler handler);

RISA_API void  risa_io_clone             (RisaIO* dest, RisaIO* src);
RISA_API bool  risa_io_should_free_input (RisaIO* io);
RISA_API void  risa_io_set_free_input    (RisaIO* io, bool value);

RISA_API char* risa_io_in             (RisaIO* io, uint8_t mode);
RISA_API void  risa_io_out            (RisaIO* io, const char* fmt, ...);
RISA_API void  risa_io_err            (RisaIO* io, const char* fmt, ...);

RISA_API char* risa_io_format         (const char* fmt, va_list list);

// Uppercase because these are the default handlers for IO.
RISA_API char* RISA_IO_STDIN          (uint8_t mode);
RISA_API void  RISA_IO_STDOUT         (const char* data);
RISA_API void  RISA_IO_STDERR         (const char* data);

#endif
