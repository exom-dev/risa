#include "dense.h"

DenseNative* dense_native_create(NativeFunction function) {
    DenseNative* native = (DenseNative*) MEM_ALLOC(sizeof(DenseNative));
    native->dense.type = DVAL_NATIVE;
    native->dense.next = NULL;
    native->function = function;

    return native;
}
