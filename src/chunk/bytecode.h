#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

#include <stdbool.h>

typedef enum {
    OP_CNST,
    OP_CNSTW,
    OP_MOV,
    OP_DGLOB,
    OP_GGLOB,
    OP_SGLOB,
    OP_UPVAL,
    OP_GUPVAL,
    OP_SUPVAL,
    OP_CUPVAL,
    OP_CLSR,
    OP_ARR,
    OP_PARR,
    OP_LEN,
    OP_OBJ,
    OP_GET,
    OP_SET,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_BNOT,
    OP_NEG,
    OP_INC,
    OP_DEC,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_SHL,
    OP_SHR,
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_EQ,
    OP_NEQ,
    OP_BAND,
    OP_BXOR,
    OP_BOR,
    OP_TEST,
    OP_NTEST,
    OP_JMP,
    OP_JMPW,
    OP_BJMP,
    OP_BJMPW,
    OP_CALL,
    OP_RET
} OpCode;

bool op_has_direct_dest(OpCode op);

#endif
