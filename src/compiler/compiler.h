#ifndef RISA_COMPILER_H
#define RISA_COMPILER_H

#include "parser.h"

#include "../chunk/chunk.h"
#include "../lexer/lexer.h"

typedef struct {
    Chunk chunk;
    Parser parser;

    uint8_t regIndex;
} Compiler;

typedef enum {
    COMPILER_OK,
    COMPILER_ERROR
} CompilerStatus;

void compiler_init(Compiler* compiler);
void compiler_delete(Compiler* compiler);

CompilerStatus compiler_compile(Compiler* compiler, const char* str);

#endif
