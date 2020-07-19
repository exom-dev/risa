#include "dense.h"

DenseUpvalue* dense_upvalue_create(Value* value) {
    DenseUpvalue* upvalue = MEM_ALLOC(sizeof(DenseUpvalue));
    upvalue->dense.type = DVAL_UPVALUE;
    upvalue->dense.link = NULL;
    upvalue->dense.marked = false;

    upvalue->ref = value;
    upvalue->closed = NULL_VALUE;

    return upvalue;
}