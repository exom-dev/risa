#ifndef RISA_COMPILER_H
#define RISA_COMPILER_H

#include "../chunk/chunk.h"

typedef enum {
    COMPILER_OK,
    COMPILER_ERROR
} CompilerStatus;

CompilerStatus compiler_compile(const char* str, Chunk* chunk);

#endif
