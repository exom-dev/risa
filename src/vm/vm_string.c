#include "vm.h"

DenseString* vm_string_create(VM* vm, const char* str, uint32_t length) {
    DenseString* string = map_find(&vm->strings, str, length, map_hash(str, length));

    if(string == NULL) {
        string = dense_string_from(str, length);
        vm_register_string(vm, string);
    }

    return string;
}

DenseString* vm_string_internalize(VM* vm, DenseString* str) {
    DenseString* string = map_find(&vm->strings, str->chars, str->length, str->hash);

    if(string == NULL) {
        string = str;
        vm_register_string(vm, string);
    } else {
        dense_delete((DenseValue*) str);
    }

    return string;
}