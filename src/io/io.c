#include "io.h"
#include "../memory/mem.h"

#include <stdio.h>

uint16_t RISA_IO_STDIN() {
    uint8_t in;

    if(fscanf(stdin, "%hhu", &in) < 1)
        return UINT16_MAX;
    return in;
}

void RISA_IO_STDOUT(const char* data) {
    fprintf(stdout, "%s", data);
}

void RISA_IO_STDERR(const char* data) {
    fprintf(stderr, "%s", data);
}

void risa_io_init(RisaIO* io) {
    io->in = &RISA_IO_STDIN;
    io->out = &RISA_IO_STDOUT;
    io->err = &RISA_IO_STDERR;
}

uint16_t risa_io_in(RisaIO* io) {
    return io->in();
}

void risa_io_out(RisaIO* io, const char* fmt, ...) {
    char* data;
    va_list args;

    va_start(args, fmt);
    data = risa_io_format(fmt, args);
    va_end(args);

    io->out(data);
    RISA_MEM_FREE(data);
}

void risa_io_err(RisaIO* io, const char* fmt, ...) {
    char* data;
    va_list args;

    va_start(args, fmt);
    data = risa_io_format(fmt, args);
    va_end(args);

    io->err(data);
    RISA_MEM_FREE(data);
}

void risa_io_redirect_in(RisaIO* io, RisaInHandler handler) {
    io->in = handler;
}

void risa_io_redirect_out(RisaIO* io, RisaOutHandler handler) {
    io->out = handler;
}

void risa_io_redirect_err(RisaIO* io, RisaOutHandler handler) {
    io->err = handler;
}

void risa_io_clone(RisaIO* dest, RisaIO* src) {
    dest->in = src->in;
    dest->out = src->out;
    dest->err = src->err;
}

char* risa_io_format(const char* fmt, va_list args) {
    char* data;

    va_list argscpy;
    va_copy(argscpy, args);

    data = RISA_MEM_ALLOC(1 + vsnprintf(NULL, 0, fmt, argscpy));

    vsprintf(data, fmt, args);
    va_end(argscpy);

    return data;
}