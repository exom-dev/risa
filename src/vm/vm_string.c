#include "vm.h"

RisaDenseString* risa_vm_string_create(RisaVM* vm, const char* str, uint32_t length) {
    RisaDenseString* string = risa_map_find(&vm->strings, str, length, risa_map_hash(str, length));

    if(string == NULL) {
        string = risa_dense_string_from(str, length);
        risa_vm_register_string(vm, string);
    }

    return string;
}

RisaDenseString* risa_vm_string_internalize(RisaVM* vm, RisaDenseString* str) {
    RisaDenseString* string = risa_map_find(&vm->strings, str->chars, str->length, str->hash);

    if(string == NULL) {
        string = str;
        risa_vm_register_string(vm, string);
    } else {
        risa_dense_delete((RisaDenseValue *) str);
    }

    return string;
}