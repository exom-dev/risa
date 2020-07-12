#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

typedef enum {
    OP_CNST,
    OP_CNSTW,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_INV,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_RET
} OpCode;

#endif
