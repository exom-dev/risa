#include "vm.h"

void vm_global_set(VM* vm, const char* str, uint32_t length, Value value) {
    map_set(&vm->globals, vm_string_create(vm, str, length), value);

    if(value.type == VAL_DENSE)
        vm_register_dense(vm, AS_DENSE(value));
}

void vm_global_set_native(VM* vm, const char* str, uint32_t length, NativeFunction fn) {
    vm_global_set(vm, str, length, dense_native_value(fn));
}