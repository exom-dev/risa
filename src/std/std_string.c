#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

static RisaValue risa_std_string_substr      (void*, uint8_t, RisaValue*);
static RisaValue risa_std_string_to_upper    (void*, uint8_t, RisaValue*);
static RisaValue risa_std_string_to_lower    (void*, uint8_t, RisaValue*);
static RisaValue risa_std_string_begins_with (void*, uint8_t, RisaValue*);
static RisaValue risa_std_string_ends_with   (void*, uint8_t, RisaValue*);

void risa_std_register_string(RisaVM* vm) {
    #define STD_STRING_ENTRY(name, fn) RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_std_string_##fn

    risa_vm_global_set_native(vm, STD_STRING_ENTRY(substr, substr));
    risa_vm_global_set_native(vm, STD_STRING_ENTRY(toUpper, to_upper));
    risa_vm_global_set_native(vm, STD_STRING_ENTRY(toLower, to_lower));
    risa_vm_global_set_native(vm, STD_STRING_ENTRY(beginsWith, begins_with));
    risa_vm_global_set_native(vm, STD_STRING_ENTRY(endsWith, ends_with));

    #undef STD_STRING_ENTRY
}

static RisaValue risa_std_string_substr(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0 || !risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING))
        return RISA_NULL_VALUE;

    if(argc == 1)
        return args[0];

    int64_t index;
    int64_t length;

    if(argc >= 2) {
        switch(args[1].type) {
            case RISA_VAL_BYTE:
                index = (int64_t) RISA_AS_BYTE(args[1]);
                break;
            case RISA_VAL_INT:
                index = RISA_AS_INT(args[1]);
                break;
            case RISA_VAL_FLOAT:
                index = (int64_t) RISA_AS_FLOAT(args[1]);
                break;
            default:
                return RISA_NULL_VALUE;
        }

        if(index < 0 || index >= RISA_AS_STRING(args[0])->length)
            return RISA_NULL_VALUE;
    } else {
        index = 0;
    }

    if(argc >= 3) {
        switch(args[2].type) {
            case RISA_VAL_BYTE:
                length = (int64_t) RISA_AS_BYTE(args[2]);
                break;
            case RISA_VAL_INT:
                length = RISA_AS_INT(args[2]);
                break;
            case RISA_VAL_FLOAT:
                length = (int64_t) RISA_AS_FLOAT(args[2]);
                break;
            default:
                return RISA_NULL_VALUE;
        }

        if(length <= 0 || (index + length) > RISA_AS_STRING(args[0])->length)
            return RISA_NULL_VALUE;
    } else {
        length = RISA_AS_STRING(args[0])->length - index;
    }

    return RISA_DENSE_VALUE(risa_vm_string_create(vm, RISA_AS_STRING(args[0])->chars + (uint32_t) index, (uint32_t) length));
}

static RisaValue risa_std_string_to_upper(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0 || !risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING))
        return RISA_NULL_VALUE;

    RisaDenseString* str = RISA_AS_STRING(args[0]);
    RisaDenseString* result = risa_dense_string_prepare(str->chars, str->length);

    for(uint32_t i = 0; i < result->length; ++i) {
        if(result->chars[i] >= 'a' && result->chars[i] <= 'z')
            result->chars[i] = result->chars[i] - 32;
    }

    risa_dense_string_hash_inplace(result);

    return RISA_DENSE_VALUE(risa_vm_string_internalize(vm, result));
}

static RisaValue risa_std_string_to_lower(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0 || !risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING))
        return RISA_NULL_VALUE;

    RisaDenseString* str = RISA_AS_STRING(args[0]);
    RisaDenseString* result = risa_dense_string_prepare(str->chars, str->length);

    for(uint32_t i = 0; i < result->length; ++i) {
        if(result->chars[i] >= 'A' && result->chars[i] <= 'Z')
            result->chars[i] = result->chars[i] + 32;
    }

    risa_dense_string_hash_inplace(result);

    return RISA_DENSE_VALUE(risa_vm_string_internalize(vm, result));
}

static RisaValue risa_std_string_begins_with(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 2 || !risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING)
                || !risa_value_is_dense_of_type(args[1], RISA_DVAL_STRING))
        return RISA_NULL_VALUE;

    RisaDenseString* str = RISA_AS_STRING(args[0]);
    RisaDenseString* substr = RISA_AS_STRING(args[1]);

    if(substr->length > str->length)
        return RISA_BOOL_VALUE(false);

    if(substr->length == str->length)
        return RISA_BOOL_VALUE(substr == str);

    for(uint32_t i = 0; i < substr->length; ++i) {
        if(substr->chars[i] != str->chars[i])
            return RISA_BOOL_VALUE(false);
    }

    return RISA_BOOL_VALUE(true);
}

static RisaValue risa_std_string_ends_with(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 2 || !risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING)
                || !risa_value_is_dense_of_type(args[1], RISA_DVAL_STRING))
        return RISA_NULL_VALUE;

    RisaDenseString* str = RISA_AS_STRING(args[0]);
    RisaDenseString* substr = RISA_AS_STRING(args[1]);

    if(substr->length > str->length)
        return RISA_BOOL_VALUE(false);

    if(substr->length == str->length)
        return RISA_BOOL_VALUE(substr == str);

    for(int64_t i = (int64_t) substr->length - 1; i >= 0; --i) {
        if(substr->chars[i] != str->chars[str->length + i - substr->length])
            return RISA_BOOL_VALUE(false);
    }

    return RISA_BOOL_VALUE(true);
}