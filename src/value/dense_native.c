#include "dense.h"

DenseNative* dense_native_create(NativeFunction function) {
    DenseNative* native = (DenseNative*) MEM_ALLOC(sizeof(DenseNative));
    native->dense.type = DVAL_NATIVE;
    native->dense.link = NULL;
    native->dense.marked = false;

    native->function = function;

    return native;
}
