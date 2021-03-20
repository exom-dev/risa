#ifndef RISA_ASM_LEXER_H_GUARD
#define RISA_ASM_LEXER_H_GUARD

#include "../def/types.h"

typedef enum {
    ASM_TOKEN_DOT, ASM_TOKEN_LEFT_PAREN, ASM_TOKEN_RIGHT_PAREN, ASM_TOKEN_LEFT_BRACE, ASM_TOKEN_RIGHT_BRACE,

    ASM_TOKEN_IDENTIFIER, ASM_TOKEN_STRING,
    ASM_TOKEN_BYTE, ASM_TOKEN_INT, ASM_TOKEN_FLOAT, ASM_TOKEN_REGISTER, ASM_TOKEN_CONSTANT,

    ASM_TOKEN_STRING_TYPE, ASM_TOKEN_BOOL_TYPE, ASM_TOKEN_BYTE_TYPE, ASM_TOKEN_INT_TYPE, ASM_TOKEN_FLOAT_TYPE, ASM_TOKEN_FUNCTION_TYPE,

    ASM_TOKEN_CODE, ASM_TOKEN_DATA,

    ASM_TOKEN_CNST, ASM_TOKEN_CNSTW,
    ASM_TOKEN_MOV,
    ASM_TOKEN_CLONE,

    ASM_TOKEN_DGLOB, ASM_TOKEN_GGLOB, ASM_TOKEN_SGLOB,

    ASM_TOKEN_UPVAL, ASM_TOKEN_GUPVAL, ASM_TOKEN_SUPVAL, ASM_TOKEN_CUPVAL, ASM_TOKEN_CLSR,

    ASM_TOKEN_ARR, ASM_TOKEN_PARR, ASM_TOKEN_LEN,
    ASM_TOKEN_OBJ, ASM_TOKEN_GET, ASM_TOKEN_SET,

    ASM_TOKEN_NULL, ASM_TOKEN_TRUE, ASM_TOKEN_FALSE,

    ASM_TOKEN_NOT, ASM_TOKEN_BNOT, ASM_TOKEN_NEG, ASM_TOKEN_INC, ASM_TOKEN_DEC,

    ASM_TOKEN_ADD, ASM_TOKEN_SUB, ASM_TOKEN_MUL, ASM_TOKEN_DIV, ASM_TOKEN_MOD, ASM_TOKEN_SHL, ASM_TOKEN_SHR,
    ASM_TOKEN_LT, ASM_TOKEN_LTE, ASM_TOKEN_EQ, ASM_TOKEN_NEQ,

    ASM_TOKEN_BAND, ASM_TOKEN_BXOR, ASM_TOKEN_BOR,

    ASM_TOKEN_TEST, ASM_TOKEN_NTEST,

    ASM_TOKEN_JMP, ASM_TOKEN_JMPW, ASM_TOKEN_BJMP, ASM_TOKEN_BJMPW,
    ASM_TOKEN_CALL, ASM_TOKEN_RET,

    ASM_TOKEN_ACC, ASM_TOKEN_DIS,

    ASM_TOKEN_ERROR,
    ASM_TOKEN_EOF
} AsmTokenType;

typedef struct {
    AsmTokenType type;
    const char* start;

    size_t size;
    uint32_t index;
} AsmToken;

typedef struct {
    const char* source;
    const char* start;
    const char* current;

    const char* stoppers;

    bool ignoreStoppers;

    size_t index;
} AsmLexer;

void asm_lexer_init(AsmLexer* lexer);
void asm_lexer_source(AsmLexer* lexer, const char* src);
void asm_lexer_delete(AsmLexer* lexer);

AsmToken asm_lexer_next(AsmLexer* lexer);
AsmToken asm_lexer_emit(AsmLexer* lexer, AsmTokenType type);
AsmToken asm_lexer_error(AsmLexer* lexer, const char* msg);

#endif
