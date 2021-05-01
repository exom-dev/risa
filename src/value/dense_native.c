#include "dense.h"

RisaDenseNative* risa_dense_native_create(RisaNativeFunction function) {
    RisaDenseNative* native = (RisaDenseNative*) RISA_MEM_ALLOC(sizeof(RisaDenseNative));
    native->dense.type = RISA_DVAL_NATIVE;
    native->dense.link = NULL;
    native->dense.marked = false;

    native->function = function;

    return native;
}

RisaValue risa_dense_native_value(RisaNativeFunction function) {
    return RISA_DENSE_VALUE(risa_dense_native_create(function));
}
