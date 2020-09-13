#ifndef RISA_ASM_LEXER_H_GUARD
#define RISA_ASM_LEXER_H_GUARD

#include "../common/headers.h"

typedef enum {
    TOKEN_DOT, TOKEN_COMMA,

    TOKEN_IDENTIFIER, TOKEN_STRING,
    TOKEN_BYTE, TOKEN_INT, TOKEN_FLOAT,

    TOKEN_CNST, TOKEN_CNSTW,
    TOKEN_MOV,
    TOKEN_CLONE,

    TOKEN_DGLOB, TOKEN_GGLOB, TOKEN_SGLOB,

    TOKEN_UPVAL, TOKEN_GUPVAL, TOKEN_SUPVAL, TOKEN_CUPVAL, TOKEN_CLSR,

    TOKEN_ARR, TOKEN_PARR, TOKEN_LEN,
    TOKEN_OBJ, TOKEN_GET, TOKEN_SET,

    TOKEN_NULL, TOKEN_TRUE, TOKEN_FALSE,

    TOKEN_NOT, TOKEN_BNOT, TOKEN_NEG, TOKEN_INC, TOKEN_DEC,

    TOKEN_ADD, TOKEN_SUB, TOKEN_MUL, TOKEN_DIV, TOKEN_MOD, TOKEN_SHL, TOKEN_SHR,
    TOKEN_GT, TOKEN_GTE, TOKEN_LT, TOKEN_LTE, TOKEN_EQ, TOKEN_NEQ,

    TOKEN_BAND, TOKEN_BXOR, TOKEN_BOR,

    TOKEN_TEST, TOKEN_NTEST,

    TOKEN_JMP, TOKEN_JMPW, TOKEN_BJMP, TOKEN_BJMPW,
    TOKEN_CALL, TOKEN_RET,

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

void lexer_init(Lexer* lexer);
void lexer_source(Lexer* lexer, const char* src);
void lexer_delete(Lexer* lexer);

Token lexer_next(Lexer* lexer);
Token lexer_emit(Lexer* lexer, TokenType type);
Token lexer_error(Lexer* lexer, const char* msg);

#endif
