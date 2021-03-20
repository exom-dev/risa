#ifndef RISA_ASSEMBLER_H_GUARD
#define RISA_ASSEMBLER_H_GUARD

#include "parser.h"
#include "lexer.h"

#include "../io/io.h"
#include "../chunk/chunk.h"
#include "../data/map.h"
#include "../def/def.h"
#include "../value/dense.h"

typedef enum {
    ASM_DATA,
    ASM_CODE
} AsmMode;

typedef struct Assembler {
    RisaIO io;
    struct Assembler* super;

    Chunk chunk;

    AsmParser* parser;
    Map* strings;

    Map identifiers;

    bool canSwitchToData;
    AsmMode mode;
} Assembler;

typedef enum {
    ASSEMBLER_OK,
    ASSEMBLER_ERROR
} AssemblerStatus;

void assembler_init(Assembler* assembler);
void assembler_delete(Assembler* assembler);

AssemblerStatus assembler_assemble(Assembler* assembler, const char* str, const char* stoppers);

#endif
