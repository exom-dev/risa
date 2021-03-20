#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

#include <stdbool.h>

#define RISA_TODLR_INSTRUCTION_SIZE 4
#define RISA_TODLR_INSTRUCTION_MASK 0x3F
#define RISA_TODLR_TYPE_MASK        0xC0
#define RISA_TODLR_TYPE_LEFT_MASK   0x80
#define RISA_TODLR_TYPE_RIGHT_MASK  0x40

typedef enum {
    OP_CNST,
    OP_CNSTW,
    OP_MOV,
    OP_CLONE,
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
    OP_RET,
    OP_ACC,
    OP_DIS
} OpCode;

bool op_has_direct_dest(OpCode op);

#endif
