#include "dense.h"

DenseUpvalue* dense_upvalue_create(Value* value) {
    DenseUpvalue* upvalue = MEM_ALLOC(sizeof(DenseUpvalue));
    upvalue->ref = value;
    upvalue->closed = NULL_VALUE;

    return upvalue;
}