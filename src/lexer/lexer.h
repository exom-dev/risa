#ifndef RISA_LEXER_H_GUARD
#define RISA_LEXER_H_GUARD

#include "../common/headers.h"

typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUNCTION, TOKEN_IF, TOKEN_NULL, TOKEN_OR,
    TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;

    size_t size;
    size_t line;
} Token;

typedef struct {
    const char* start;
    const char* current;

    size_t line;
} Lexer;

Lexer* lexer_create();
void   lexer_init(Lexer* lexer);
void   lexer_source(Lexer* lexer, const char* src);
void   lexer_delete(Lexer* lexer);

Token  lexer_next(Lexer* lexer);
Token  lexer_emit(Lexer* lexer, TokenType type);
Token  lexer_error(Lexer* lexer, const char* msg);

#endif
