#include "dense.h"

RisaDenseFunction* risa_dense_function_create() {
    RisaDenseFunction* function = (RisaDenseFunction*) RISA_MEM_ALLOC(sizeof(RisaDenseFunction));

    risa_dense_function_init(function);
    return function;
}

void risa_dense_function_init(RisaDenseFunction* function) {
    function->dense.type = RISA_DVAL_FUNCTION;
    function->dense.link = NULL;
    function->dense.marked = false;

    function->arity = 0;
    function->name = NULL;
    cluster_init(&function->cluster);
}

void risa_dense_function_delete(RisaDenseFunction* function) {
    cluster_delete(&function->cluster);
    risa_dense_function_init(function);
}
