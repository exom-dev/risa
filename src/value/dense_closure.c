#include "dense.h"

RisaDenseClosure* risa_dense_closure_create(RisaDenseFunction* function, uint8_t upvalueCount) {
    RisaDenseUpvalue** upvalues = RISA_MEM_ALLOC(upvalueCount * sizeof(RisaDenseUpvalue));
    for(uint8_t i = 0; i < upvalueCount; ++i)
        upvalues[i] = NULL;

    RisaDenseClosure* closure = RISA_MEM_ALLOC(sizeof(RisaDenseClosure));
    closure->dense.type = RISA_DVAL_CLOSURE;
    closure->dense.link = NULL;
    closure->dense.marked = false;

    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = upvalueCount;

    return closure;
}