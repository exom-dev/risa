#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

#include <string.h>

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

static Value std_io_read_char(void* vm, uint8_t argc, Value* args) {
    char* chr = risa_io_in(&((VM*) vm)->io, RISA_INPUT_CHAR);

    if(chr == NULL)
        return NULL_VALUE;

    Value result = DENSE_VALUE((DenseString*) vm_string_create(vm, chr, 1));

    if(((VM*) vm)->io.freeInput)
        RISA_MEM_FREE(chr);

    return result;
}

static Value std_io_read_string(void* vm, uint8_t argc, Value* args) {
    char* str = risa_io_in(&((VM*) vm)->io, RISA_INPUT_WORD);

    if(str == NULL)
        return NULL_VALUE;

    uint32_t size = (uint32_t) strlen(str);

    Value result = DENSE_VALUE((DenseString*) vm_string_create(vm, str, size));

    if(((VM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}

static Value std_io_read_line(void* vm, uint8_t argc, Value* args) {
    char* line = risa_io_in(&((VM*) vm)->io, RISA_INPUT_LINE);

    if(line == NULL)
        return NULL_VALUE;

    uint32_t size = (uint32_t) strlen(line);

    Value result = DENSE_VALUE((DenseString*) vm_string_create(vm, line, size));

    if(((VM*) vm)->io.freeInput)
        RISA_MEM_FREE(line);

    return result;
}

void std_register_io(VM* vm) {
    #define STD_IO_ENTRY(name)           RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, std_io_##name
    #define STD_IO_OBJ_ENTRY(name, fn) , RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, dense_native_value(std_io_##fn)

    DenseObject* objRead = dense_object_create_with(vm, 3
                                                    STD_IO_OBJ_ENTRY(char, read_char)
                                                    STD_IO_OBJ_ENTRY(string, read_string)
                                                    STD_IO_OBJ_ENTRY(line, read_line));

    vm_global_set_native(vm, STD_IO_ENTRY(print));
    vm_global_set_native(vm, STD_IO_ENTRY(println));

    vm_global_set(vm, "read", sizeof("read") - 1, DENSE_VALUE((DenseValue*) objRead));

    #undef STD_IO_OBJ_ENTRY
    #undef STD_IO_ENTRY
}