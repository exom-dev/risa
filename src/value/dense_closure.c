#include "dense.h"

DenseClosure* dense_closure_create(DenseFunction* function, uint8_t upvalueCount) {
    DenseUpvalue** upvalues = MEM_ALLOC(upvalueCount * sizeof(DenseUpvalue));
    for(uint8_t i = 0; i < upvalueCount; ++i)
        upvalues[i] = NULL;

    DenseClosure* closure = MEM_ALLOC(sizeof(DenseClosure));
    closure->dense.type = DVAL_CLOSURE;
    closure->dense.link = NULL;
    closure->dense.marked = false;

    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = upvalueCount;

    return closure;
}