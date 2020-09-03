#ifndef RISA_LEXER_H_GUARD
#define RISA_LEXER_H_GUARD

#include "../common/headers.h"

typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_MINUS_MINUS, TOKEN_PLUS, TOKEN_PLUS_PLUS,
    TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    TOKEN_TILDE, TOKEN_CARET, TOKEN_PERCENT, TOKEN_QUESTION,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_EQUAL_GREATER,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_LESS_LESS,
    TOKEN_AMPERSAND, TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_PIPE, TOKEN_PIPE_PIPE,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_BYTE, TOKEN_INT, TOKEN_FLOAT,

    TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL,
    TOKEN_VAR, TOKEN_FUNCTION, TOKEN_RETURN,
    TOKEN_CONTINUE, TOKEN_BREAK,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;

    size_t size;
    uint32_t index;
} Token;

typedef struct {
    const char* source;
    const char* start;
    const char* current;

    size_t index;
} Lexer;

void  lexer_init(Lexer* lexer);
void  lexer_source(Lexer* lexer, const char* src);
void  lexer_delete(Lexer* lexer);

Token lexer_next(Lexer* lexer);
Token lexer_emit(Lexer* lexer, TokenType type);
Token lexer_error(Lexer* lexer, const char* msg);

bool identifier_equals(Token* left, Token* right);

#endif
