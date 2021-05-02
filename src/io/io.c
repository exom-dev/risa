#include "io.h"
#include "../mem/mem.h"
#include "../def/def.h"

#include <stdio.h>
#include <string.h>

char* RISA_IO_STDIN(uint8_t mode) {
    switch(mode) {
        case RISA_INPUT_MODE_CHAR: {
            int data = getc(stdin);

            if(data == EOF)
                return NULL;

            char* chr = RISA_MEM_ALLOC(sizeof(char) * 2);
            *chr = (char) data;

            return chr;
        }
        case RISA_INPUT_MODE_WORD: {
            char* word = RISA_MEM_ALLOC(sizeof(char) * (RISA_INPUT_WORD_BUFFER_SIZE + 1));

            if(fscanf(stdin, "%" RISA_STRINGIFY(RISA_INPUT_WORD_BUFFER_SIZE) "s", word) < 1) {
                RISA_MEM_FREE(word);
                return NULL;
            }

            return word;
        }
        case RISA_INPUT_MODE_LINE: {
            char* line = RISA_MEM_ALLOC(sizeof(char) * (RISA_INPUT_LINE_BUFFER_SIZE + 1));

            if(fgets(line, RISA_INPUT_LINE_BUFFER_SIZE + 1, stdin) == NULL) {
                RISA_MEM_FREE(line);
                return NULL;
            }

            size_t length = strlen(line);

            if(line[length - 1] == '\n')
                line[length - 1] = '\0';
            if(line[length - 2] == '\r')
                line[length - 2] = '\0';

            return line;
        }
        default:
            return NULL;
    }
}

void RISA_IO_STDOUT(const char* data) {
    fprintf(stdout, "%s", data);
}

void RISA_IO_STDERR(const char* data) {
    fprintf(stderr, "%s", data);
}

RisaIO* risa_io_create() {
    RisaIO* io = RISA_MEM_ALLOC(sizeof(RisaIO));

    risa_io_init(io);

    return io;
}

void risa_io_init(RisaIO* io) {
    io->in = &RISA_IO_STDIN;
    io->out = &RISA_IO_STDOUT;
    io->err = &RISA_IO_STDERR;
    io->freeInput = true;
}

void risa_io_free(RisaIO* io) {
    RISA_MEM_FREE(io);
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
    dest->freeInput = src->freeInput;
}

bool risa_io_should_free_input (RisaIO* io) {
    return (io->in == RISA_IO_STDIN) || (io->freeInput);
}

void risa_io_set_free_input(RisaIO* io, bool value) {
    io->freeInput = value;
}

char* risa_io_in(RisaIO* io, uint8_t mode) {
    return io->in(mode);
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

char* risa_io_format(const char* fmt, va_list args) {
    char* data;

    va_list argscpy;
    va_copy(argscpy, args);

    size_t size = 1 + vsnprintf(NULL, 0, fmt, argscpy);

    data = RISA_MEM_ALLOC(size);

    vsnprintf(data, size, fmt, args);
    va_end(argscpy);

    return data;
}