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
    return risa_value_from_dense(risa_dense_native_create(function));
}

RisaValue risa_dense_native_get_arg(RisaValue* args, uint8_t index) {
    return args[index];
}

RisaValue* risa_dense_native_get_base(RisaValue* args, uint8_t argc) {
    return args + argc;
}