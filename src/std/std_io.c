#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

#include <string.h>
#include <stdlib.h>

static RisaValue risa_std_io_print       (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_println     (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_char   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_string (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_line   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_int    (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_byte   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_float  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_io_read_bool   (void*, uint8_t, RisaValue*);

void risa_std_register_io(RisaVM* vm) {
    #define STD_IO_ENTRY(name)           RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_std_io_##name
    #define STD_IO_OBJ_ENTRY(name, fn) , RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_dense_native_value(risa_std_io_##fn)

    #undef bool // This gets treated as a macro, which changes the 'bool' entry to something else.

    RisaDenseObject* objRead = risa_dense_object_create_under(vm, 7
                                                              STD_IO_OBJ_ENTRY(char, read_char)
                                                              STD_IO_OBJ_ENTRY(string, read_string)
                                                              STD_IO_OBJ_ENTRY(line, read_line)
                                                              STD_IO_OBJ_ENTRY(int, read_int)
                                                              STD_IO_OBJ_ENTRY(byte, read_byte)
                                                              STD_IO_OBJ_ENTRY(float, read_float)
                                                              STD_IO_OBJ_ENTRY(bool, read_bool));

    risa_vm_global_set_native(vm, STD_IO_ENTRY(print));
    risa_vm_global_set_native(vm, STD_IO_ENTRY(println));

    risa_vm_global_set(vm, "read", sizeof("read") - 1, RISA_DENSE_VALUE((RisaDenseValue *) objRead));

    #undef STD_IO_OBJ_ENTRY
    #undef STD_IO_ENTRY
}

static RisaValue risa_std_io_print(void* vm, uint8_t argc, RisaValue* args) {
    for(uint16_t i = 0; i < argc; ++i)
        risa_value_print(&((RisaVM *) vm)->io, args[i]);

    return RISA_NULL_VALUE;
}

static RisaValue risa_std_io_println(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0) {
        risa_io_out(&((RisaVM*) vm)->io, "\n");
    } else {
        for(uint16_t i = 0; i < argc; ++i) {
            risa_value_print(&((RisaVM *) vm)->io, args[i]);
            risa_io_out(&((RisaVM*) vm)->io, "\n");
        }
    }

    return RISA_NULL_VALUE;
}

static RisaValue risa_std_io_read_char(void* vm, uint8_t argc, RisaValue* args) {
    char* chr = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_CHAR);

    if(chr == NULL)
        return RISA_NULL_VALUE;

    RisaValue result = RISA_DENSE_VALUE((RisaDenseString*) risa_vm_string_create(vm, chr, 1));

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(chr);

    return result;
}

static RisaValue risa_std_io_read_string(void* vm, uint8_t argc, RisaValue* args) {
    char* str = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_WORD);

    if(str == NULL)
        return RISA_NULL_VALUE;

    uint32_t size = (uint32_t) strlen(str);

    RisaValue result = RISA_DENSE_VALUE((RisaDenseString*) risa_vm_string_create(vm, str, size));

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}

static RisaValue risa_std_io_read_line(void* vm, uint8_t argc, RisaValue* args) {
    char* line = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_LINE);

    if(line == NULL)
        return RISA_NULL_VALUE;

    // If there is a line ending left from a previous read operation, ignore and read again.
    if(*line == '\0')
        line = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_LINE);

    if(line == NULL)
        return RISA_NULL_VALUE;

    uint32_t size = (uint32_t) strlen(line);

    RisaValue result = RISA_DENSE_VALUE((RisaDenseString*) risa_vm_string_create(vm, line, size));

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(line);

    return result;
}

static RisaValue risa_std_io_read_int(void* vm, uint8_t argc, RisaValue* args) {
    char* str = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_WORD);

    if(str == NULL)
        return RISA_NULL_VALUE;

    uint32_t length = (uint32_t) strlen(str);

    RisaValue result = risa_value_int_from_string(str, length);

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}

static RisaValue risa_std_io_read_byte(void* vm, uint8_t argc, RisaValue* args) {
    char* str = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_WORD);

    if(str == NULL)
        return RISA_NULL_VALUE;

    uint32_t length = (uint32_t) strlen(str);

    RisaValue result = risa_value_byte_from_string(str, length);

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}

static RisaValue risa_std_io_read_float(void* vm, uint8_t argc, RisaValue* args) {
    char* str = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_WORD);

    if(str == NULL)
        return RISA_NULL_VALUE;

    RisaValue result = risa_value_float_from_string(str);

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}

static RisaValue risa_std_io_read_bool(void* vm, uint8_t argc, RisaValue* args) {
    char* str = risa_io_in(&((RisaVM*) vm)->io, RISA_INPUT_MODE_WORD);

    if(str == NULL)
        return RISA_NULL_VALUE;

    RisaValue result = risa_value_bool_from_string(str);

    if(((RisaVM*) vm)->io.freeInput)
        RISA_MEM_FREE(str);

    return result;
}