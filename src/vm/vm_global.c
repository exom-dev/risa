#include "vm.h"

void risa_vm_global_set(RisaVM* vm, const char* str, uint32_t length, RisaValue value) {
    risa_map_set(&vm->globals, risa_vm_string_create(vm, str, length), value);

    if(value.type == RISA_VAL_DENSE)
        risa_vm_register_dense(vm, AS_DENSE(value));
}

void risa_vm_global_set_native(RisaVM* vm, const char* str, uint32_t length, RisaNativeFunction fn) {
    risa_vm_global_set(vm, str, length, risa_dense_native_value(fn));
}