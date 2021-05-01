#ifndef RISA_ASM_LEXER_H_GUARD
#define RISA_ASM_LEXER_H_GUARD

#include "../api.h"
#include "../def/types.h"
#include "../cluster/bytecode.h"

typedef enum {
    RISA_ASM_TOKEN_DOT, RISA_ASM_TOKEN_LEFT_PAREN, RISA_ASM_TOKEN_RIGHT_PAREN, RISA_ASM_TOKEN_LEFT_BRACE, RISA_ASM_TOKEN_RIGHT_BRACE,

    RISA_ASM_TOKEN_IDENTIFIER, RISA_ASM_TOKEN_STRING,
    RISA_ASM_TOKEN_BYTE, RISA_ASM_TOKEN_INT, RISA_ASM_TOKEN_FLOAT, RISA_ASM_TOKEN_REGISTER, RISA_ASM_TOKEN_CONSTANT,

    RISA_ASM_TOKEN_STRING_TYPE, RISA_ASM_TOKEN_BOOL_TYPE, RISA_ASM_TOKEN_BYTE_TYPE, RISA_ASM_TOKEN_INT_TYPE, RISA_ASM_TOKEN_FLOAT_TYPE, RISA_ASM_TOKEN_FUNCTION_TYPE,

    RISA_ASM_TOKEN_CODE, RISA_ASM_TOKEN_DATA,

    RISA_ASM_TOKEN_CNST, RISA_ASM_TOKEN_CNSTW,
    RISA_ASM_TOKEN_MOV,
    RISA_ASM_TOKEN_CLONE,

    RISA_ASM_TOKEN_DGLOB, RISA_ASM_TOKEN_GGLOB, RISA_ASM_TOKEN_SGLOB,

    RISA_ASM_TOKEN_UPVAL, RISA_ASM_TOKEN_GUPVAL, RISA_ASM_TOKEN_SUPVAL, RISA_ASM_TOKEN_CUPVAL, RISA_ASM_TOKEN_CLSR,

    RISA_ASM_TOKEN_ARR, RISA_ASM_TOKEN_PARR, RISA_ASM_TOKEN_LEN,
    RISA_ASM_TOKEN_OBJ, RISA_ASM_TOKEN_GET, RISA_ASM_TOKEN_SET,

    RISA_ASM_TOKEN_NULL, RISA_ASM_TOKEN_TRUE, RISA_ASM_TOKEN_FALSE,

    RISA_ASM_TOKEN_NOT, RISA_ASM_TOKEN_BNOT, RISA_ASM_TOKEN_NEG, RISA_ASM_TOKEN_INC, RISA_ASM_TOKEN_DEC,

    RISA_ASM_TOKEN_ADD, RISA_ASM_TOKEN_SUB, RISA_ASM_TOKEN_MUL, RISA_ASM_TOKEN_DIV, RISA_ASM_TOKEN_MOD, RISA_ASM_TOKEN_SHL, RISA_ASM_TOKEN_SHR,
    RISA_ASM_TOKEN_LT, RISA_ASM_TOKEN_LTE, RISA_ASM_TOKEN_EQ, RISA_ASM_TOKEN_NEQ,

    RISA_ASM_TOKEN_BAND, RISA_ASM_TOKEN_BXOR, RISA_ASM_TOKEN_BOR,

    RISA_ASM_TOKEN_TEST, RISA_ASM_TOKEN_NTEST,

    RISA_ASM_TOKEN_JMP, RISA_ASM_TOKEN_JMPW, RISA_ASM_TOKEN_BJMP, RISA_ASM_TOKEN_BJMPW,
    RISA_ASM_TOKEN_CALL, RISA_ASM_TOKEN_RET,

    RISA_ASM_TOKEN_ACC, RISA_ASM_TOKEN_DIS,

    RISA_ASM_TOKEN_ERROR,
    RISA_ASM_TOKEN_EOF
} RisaAsmTokenType;

typedef struct {
    RisaAsmTokenType type;
    const char* start;

    size_t size;
    uint32_t index;
} RisaAsmToken;

typedef struct {
    const char* source;
    const char* start;
    const char* current;

    const char* stoppers;

    bool ignoreStoppers;

    size_t index;
} RisaAsmLexer;

RISA_API RisaOpCode   risa_asm_token_to_opcode (RisaAsmTokenType token);

RISA_API void         risa_asm_lexer_init      (RisaAsmLexer* lexer);
RISA_API void         risa_asm_lexer_source    (RisaAsmLexer* lexer, const char* src);
RISA_API void         risa_asm_lexer_delete    (RisaAsmLexer* lexer);

RISA_API RisaAsmToken risa_asm_lexer_next      (RisaAsmLexer* lexer);
RISA_API RisaAsmToken risa_asm_lexer_emit      (RisaAsmLexer* lexer, RisaAsmTokenType type);
RISA_API RisaAsmToken risa_asm_lexer_error     (RisaAsmLexer* lexer, const char* msg);

#endif
