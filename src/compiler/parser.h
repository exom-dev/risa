#ifndef RISA_PARSER_H_GUARD
#define RISA_PARSER_H_GUARD

#include "../lexer/lexer.h"
#include "../common/headers.h"

typedef struct {
    Lexer lexer;

    Token current;
    Token previous;

    bool error;
    bool panic;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // ||
    PREC_AND,        // &&
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! - ~
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

void parser_init(Parser* parser);
void parser_advance(Parser* parser);
void parser_consume(Parser* parser, TokenType type, const char* err);

void parser_error_at(Parser* parser, Token token, const char* msg);
void parser_error_at_current(Parser* parser, const char* msg);
void parser_error_at_previous(Parser* parser, const char* msg);

#endif
