#ifndef RISA_BYTECODE_H_GUARD
#define RISA_BYTECODE_H_GUARD

#include "../api.h"
#include "../def/types.h"
#include "../def/macro.h"

#define RISA_TODLR_INSTRUCTION_SIZE 4
#define RISA_TODLR_INSTRUCTION_MASK  0x3F

#define RISA_TODLR_TYPE_MASK         0xC0
#define RISA_TODLR_TYPE_LEFT_MASK    0x80
#define RISA_TODLR_TYPE_RIGHT_MASK   0x40

// About REGISTER_NULL:
// Some instructions expect either a register or this value. Examples:
//
// RET 20r means 'return the value stored in the 20th register'.         RET 251 means 'return null'.
// DIS 20r means 'disassemble the function stored in the 20th register'. DIS 251 means 'disassemble the current function'.
#define RISA_TODLR_REGISTER_COUNT    250
#define RISA_TODLR_REGISTER_NULL     251
#define RISA_TODLR_REGISTER_NULL_STR RISA_STRINGIFY(RISA_TODLR_REGISTER_NULL)

typedef enum {
    RISA_OP_CNST,
    RISA_OP_CNSTW,
    RISA_OP_MOV,
    RISA_OP_CLONE,
    RISA_OP_DGLOB,
    RISA_OP_GGLOB,
    RISA_OP_SGLOB,
    RISA_OP_UPVAL,
    RISA_OP_GUPVAL,
    RISA_OP_SUPVAL,
    RISA_OP_CUPVAL,
    RISA_OP_CLSR,
    RISA_OP_ARR,
    RISA_OP_PARR,
    RISA_OP_LEN,
    RISA_OP_OBJ,
    RISA_OP_GET,
    RISA_OP_SET,
    RISA_OP_NULL,
    RISA_OP_TRUE,
    RISA_OP_FALSE,
    RISA_OP_NOT,
    RISA_OP_BNOT,
    RISA_OP_NEG,
    RISA_OP_INC,
    RISA_OP_DEC,
    RISA_OP_ADD,
    RISA_OP_SUB,
    RISA_OP_MUL,
    RISA_OP_DIV,
    RISA_OP_MOD,
    RISA_OP_SHL,
    RISA_OP_SHR,
    RISA_OP_LT,
    RISA_OP_LTE,
    RISA_OP_EQ,
    RISA_OP_NEQ,
    RISA_OP_BAND,
    RISA_OP_BXOR,
    RISA_OP_BOR,
    RISA_OP_TEST,
    RISA_OP_NTEST,
    RISA_OP_JMP,
    RISA_OP_JMPW,
    RISA_OP_BJMP,
    RISA_OP_BJMPW,
    RISA_OP_CALL,
    RISA_OP_RET,
    RISA_OP_ACC,
    RISA_OP_DIS
} RisaOpCode;

// Wether or not an operation has a direct register destination (e.g. MOV, ADD, ...)
// Used for optimizations (e.g. to move the result directly in a local variable without an extra MOV)
RISA_API bool risa_op_has_direct_dest (RisaOpCode op);

#endif
