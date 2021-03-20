#include "dense.h"

DenseFunction* dense_function_create() {
    DenseFunction* function = (DenseFunction*) RISA_MEM_ALLOC(sizeof(DenseFunction));

    dense_function_init(function);
    return function;
}

void dense_function_init(DenseFunction* function) {
    function->dense.type = DVAL_FUNCTION;
    function->dense.link = NULL;
    function->dense.marked = false;

    function->arity = 0;
    function->name = NULL;
    chunk_init(&function->chunk);
}

void dense_function_delete(DenseFunction* function) {
    chunk_delete(&function->chunk);
    dense_function_init(function);
}
