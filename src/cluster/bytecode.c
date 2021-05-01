#include "bytecode.h"

bool risa_op_has_direct_dest(RisaOpCode op) {
    switch(op) {
        case RISA_OP_CNST:
        case RISA_OP_CNSTW:
        case RISA_OP_MOV:
        case RISA_OP_GGLOB:
        case RISA_OP_GUPVAL:
        case RISA_OP_ARR:
        case RISA_OP_LEN:
        case RISA_OP_OBJ:
        case RISA_OP_GET:
        case RISA_OP_NULL:
        case RISA_OP_TRUE:
        case RISA_OP_FALSE:
        case RISA_OP_NOT:
        case RISA_OP_BNOT:
        case RISA_OP_NEG:
        case RISA_OP_ADD:
        case RISA_OP_SUB:
        case RISA_OP_MUL:
        case RISA_OP_DIV:
        case RISA_OP_MOD:
        case RISA_OP_SHL:
        case RISA_OP_SHR:
        case RISA_OP_LT:
        case RISA_OP_LTE:
        case RISA_OP_EQ:
        case RISA_OP_NEQ:
        case RISA_OP_BAND:
        case RISA_OP_BXOR:
        case RISA_OP_BOR:
            return true;
        default:
            return false;
    }
}
