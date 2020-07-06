#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

typedef enum {
    OP_CNST,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_RET
} OpCode;

#endif
