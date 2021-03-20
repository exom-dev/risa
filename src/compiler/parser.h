#ifndef RISA_PARSER_H_GUARD
#define RISA_PARSER_H_GUARD

#include "../io/io.h"
#include "../lexer/lexer.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    Lexer lexer;

    Token current;
    Token previous;

    bool error;
    bool panic;
} Parser;

void parser_init(Parser* parser);
void parser_advance(Parser* parser);
void parser_consume(Parser* parser, TokenType type, const char* err);

void parser_sync(Parser* parser);

void parser_error_at(Parser* parser, Token token, const char* msg);
void parser_error_at_current(Parser* parser, const char* msg);
void parser_error_at_previous(Parser* parser, const char* msg);

#endif
