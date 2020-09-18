#ifndef RISA_ASM_PARSER_H_GUARD
#define RISA_ASM_PARSER_H_GUARD

#include "lexer.h"
#include "../common/headers.h"

typedef struct {
    Lexer lexer;

    Token current;
    Token previous;

    bool error;
    bool panic;
} Parser;

void asm_parser_init(Parser* parser);
void asm_parser_advance(Parser* parser);
void asm_parser_consume(Parser* parser, TokenType type, const char* err);

void asm_parser_sync(Parser* parser);
void asm_parser_error_at(Parser* parser, Token token, const char* msg);
void asm_parser_error_at_previous(Parser* parser, const char* msg);
void asm_parser_error_at_current(Parser* parser, const char* msg);

#endif
