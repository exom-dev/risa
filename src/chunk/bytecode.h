#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

typedef enum {
    OP_CNST,
    OP_CNSTW,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_BNOT,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
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
    OP_RET
} OpCode;

#endif
