#include "dense.h"

RisaDenseUpvalue* risa_dense_upvalue_create(RisaValue* value) {
    RisaDenseUpvalue* upvalue = RISA_MEM_ALLOC(sizeof(RisaDenseUpvalue));
    upvalue->dense.type = RISA_DVAL_UPVALUE;
    upvalue->dense.link = NULL;
    upvalue->dense.marked = false;

    upvalue->ref = value;
    upvalue->closed = risa_value_from_null();

    return upvalue;
}