#include "vm.h"

DenseString* vm_string_internalize(VM* vm, const char* str, uint32_t length) {
    DenseString* string = map_find(&vm->strings, str, length, map_hash(str, length));

    if(string == NULL) {
        string = dense_string_from(str, length);
        vm_register_string(vm, string);
    }

    return string;
}
