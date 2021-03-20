#ifndef RISA_ASM_PARSER_H_GUARD
#define RISA_ASM_PARSER_H_GUARD

#include "lexer.h"
#include "../io/io.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    AsmLexer lexer;

    AsmToken current;
    AsmToken previous;

    bool error;
    bool panic;
} AsmParser;

void asm_parser_init(AsmParser* parser);
void asm_parser_advance(AsmParser* parser);
void asm_parser_consume(AsmParser* parser, AsmTokenType type, const char* err);

void asm_parser_sync(AsmParser* parser);
void asm_parser_error_at(AsmParser* parser, AsmToken token, const char* msg);
void asm_parser_error_at_previous(AsmParser* parser, const char* msg);
void asm_parser_error_at_current(AsmParser* parser, const char* msg);

#endif
