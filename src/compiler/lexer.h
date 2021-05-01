#ifndef RISA_LEXER_H_GUARD
#define RISA_LEXER_H_GUARD

#include "../api.h"
#include "../def/types.h"

// Note: when adding a new token, make sure to add an entry for it in the big operator rules table (./compiler.c - const RisaOperatorRule OPERATOR_RULES[]).
// Make sure that you add it in the proper position (the first row is for the first token, the second one is for the second, etc).
// Not doing so will result in bad things happening. The compiler assumes that the table is correct, complete, and ordered..
typedef enum {
    RISA_TOKEN_LEFT_PAREN, RISA_TOKEN_RIGHT_PAREN,
    RISA_TOKEN_LEFT_BRACKET, RISA_TOKEN_RIGHT_BRACKET,
    RISA_TOKEN_LEFT_BRACE, RISA_TOKEN_RIGHT_BRACE,
    RISA_TOKEN_COMMA, RISA_TOKEN_DOT, RISA_TOKEN_MINUS, RISA_TOKEN_MINUS_MINUS, RISA_TOKEN_PLUS, RISA_TOKEN_PLUS_PLUS,
    RISA_TOKEN_COLON, RISA_TOKEN_SEMICOLON, RISA_TOKEN_SLASH, RISA_TOKEN_STAR,
    RISA_TOKEN_TILDE, RISA_TOKEN_CARET, RISA_TOKEN_PERCENT, RISA_TOKEN_QUESTION,
    RISA_TOKEN_DOLLAR,

    RISA_TOKEN_BANG, RISA_TOKEN_BANG_EQUAL,
    RISA_TOKEN_EQUAL, RISA_TOKEN_EQUAL_EQUAL, RISA_TOKEN_EQUAL_GREATER,
    RISA_TOKEN_PLUS_EQUAL, RISA_TOKEN_MINUS_EQUAL, RISA_TOKEN_STAR_EQUAL, RISA_TOKEN_SLASH_EQUAL,
    RISA_TOKEN_CARET_EQUAL, RISA_TOKEN_PERCENT_EQUAL, RISA_TOKEN_PIPE_EQUAL, RISA_TOKEN_AMPERSAND_EQUAL,
    RISA_TOKEN_GREATER, RISA_TOKEN_GREATER_EQUAL, RISA_TOKEN_GREATER_GREATER,
    RISA_TOKEN_LESS, RISA_TOKEN_LESS_EQUAL, RISA_TOKEN_LESS_LESS,
    RISA_TOKEN_AMPERSAND, RISA_TOKEN_AMPERSAND_AMPERSAND,
    RISA_TOKEN_PIPE, RISA_TOKEN_PIPE_PIPE,

    RISA_TOKEN_IDENTIFIER, RISA_TOKEN_STRING, RISA_TOKEN_BYTE, RISA_TOKEN_INT, RISA_TOKEN_FLOAT,

    RISA_TOKEN_IF, RISA_TOKEN_ELSE, RISA_TOKEN_WHILE, RISA_TOKEN_FOR,
    RISA_TOKEN_TRUE, RISA_TOKEN_FALSE, RISA_TOKEN_NULL,
    RISA_TOKEN_VAR, RISA_TOKEN_FUNCTION, RISA_TOKEN_RETURN,
    RISA_TOKEN_CONTINUE, RISA_TOKEN_BREAK,
    RISA_TOKEN_CLONE,

    RISA_TOKEN_ERROR,
    RISA_TOKEN_EOF
} RisaTokenType;

typedef struct {
    RisaTokenType type;
    const char* start;

    size_t size;
    uint32_t index;
} RisaToken;

typedef struct {
    const char* source;
    const char* start;
    const char* current;

    size_t index;
} RisaLexer;

RISA_API void      risa_lexer_init              (RisaLexer* lexer);
RISA_API void      risa_lexer_source            (RisaLexer* lexer, const char* src);
RISA_API void      risa_lexer_delete            (RisaLexer* lexer);

RISA_API RisaToken risa_lexer_next              (RisaLexer* lexer);
RISA_API RisaToken risa_lexer_emit              (RisaLexer* lexer, RisaTokenType type);
RISA_API RisaToken risa_lexer_error             (RisaLexer* lexer, const char* msg);

RISA_API bool      risa_token_identifier_equals (RisaToken* left, RisaToken* right);

#endif
