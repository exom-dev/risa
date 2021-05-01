#include "dense.h"

RisaDenseUpvalue* risa_dense_upvalue_create(RisaValue* value) {
    RisaDenseUpvalue* upvalue = RISA_MEM_ALLOC(sizeof(RisaDenseUpvalue));
    upvalue->dense.type = RISA_DVAL_UPVALUE;
    upvalue->dense.link = NULL;
    upvalue->dense.marked = false;

    upvalue->ref = value;
    upvalue->closed = RISA_NULL_VALUE;

    return upvalue;
}