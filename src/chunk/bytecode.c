#include "bytecode.h"

bool op_has_direct_dest(OpCode op) {
    switch(op) {
        case OP_MOV:
        case OP_GGLOB:
        case OP_GUPVAL:
        case OP_ARR:
        case OP_LEN:
        case OP_GET:
        case OP_NULL:
        case OP_TRUE:
        case OP_FALSE:
        case OP_NOT:
        case OP_BNOT:
        case OP_NEG:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_SHL:
        case OP_SHR:
        case OP_GT:
        case OP_GTE:
        case OP_LT:
        case OP_LTE:
        case OP_EQ:
        case OP_NEQ:
        case OP_BAND:
        case OP_BXOR:
        case OP_BOR:
            return true;
        default:
            return false;
    }
}
