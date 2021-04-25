#include "std.h"
#include "../value/value.h"

static Value std_io_print(void* vm, uint8_t argc, Value* args) {
    for(uint16_t i = 0; i < argc; ++i)
        value_print(&((VM *) vm)->io, args[i]);

    return NULL_VALUE;
}

static Value std_io_println(void* vm, uint8_t argc, Value* args) {
    if(argc == 0) {
        risa_io_out(&((VM*) vm)->io, "\n");
    } else {
        for(uint16_t i = 0; i < argc; ++i) {
            value_print(&((VM *) vm)->io, args[i]);
            risa_io_out(&((VM*) vm)->io, "\n");
        }
    }

    return NULL_VALUE;
}

void std_register_io(VM* vm) {
    vm_global_set_native(vm, "print", sizeof("print") - 1, std_io_print);
    vm_global_set_native(vm, "println", sizeof("println") - 1, std_io_println);
}