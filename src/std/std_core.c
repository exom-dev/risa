#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

#include <string.h>

static RisaValue risa_std_core_typeof    (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_to_string (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_to_int    (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_to_byte   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_to_float  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_to_bool   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_core_foreach   (void*, uint8_t, RisaValue*);

static RisaValue risa_std_core_typeof_internal (RisaVM*, RisaValue);

void risa_std_register_core(RisaVM* vm) {
    #define STD_CORE_ENTRY(name, fn) RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, risa_std_core_##fn

    risa_vm_global_set_native(vm, STD_CORE_ENTRY(typeof, typeof));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(toString, to_string));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(toInt, to_int));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(toByte, to_byte));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(toFloat, to_float));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(toBool, to_bool));
    risa_vm_global_set_native(vm, STD_CORE_ENTRY(foreach, foreach));

    #undef STD_CORE_ENTRY
}

static RisaValue risa_std_core_typeof(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    return risa_std_core_typeof_internal((RisaVM *) vm, args[0]);
}

static RisaValue risa_std_core_to_string(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    char* str = value_to_string(args[0]);

    RisaDenseString* result = risa_vm_string_create(vm, str, strlen(str));

    RISA_MEM_FREE(str);

    return DENSE_VALUE((RisaDenseValue*) result);
}

static RisaValue risa_std_core_to_int(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case RISA_VAL_NULL: {
            return NULL_VALUE;
        }
        case RISA_VAL_BOOL: {
            return AS_BOOL(args[0]) ? INT_VALUE(1) : INT_VALUE(0);
        }
        case RISA_VAL_BYTE: {
            return INT_VALUE((int64_t) AS_BYTE(args[0]));
        }
        case RISA_VAL_INT: {
            return args[0];
        }
        case RISA_VAL_FLOAT: {
            return INT_VALUE((int64_t) AS_FLOAT(args[0]));
        }
        case RISA_VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case RISA_DVAL_STRING: {
                    return value_int_from_string(AS_STRING(args[0])->chars, AS_STRING(args[0])->length);
                }
                case RISA_DVAL_ARRAY:
                case RISA_DVAL_OBJECT:
                case RISA_DVAL_UPVALUE:
                case RISA_DVAL_FUNCTION:
                case RISA_DVAL_CLOSURE:
                case RISA_DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static RisaValue risa_std_core_to_byte(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case RISA_VAL_NULL: {
            return NULL_VALUE;
        }
        case RISA_VAL_BOOL: {
            return AS_BOOL(args[0]) ? BYTE_VALUE(1) : BYTE_VALUE(0);
        }
        case RISA_VAL_BYTE: {
            return args[0];
        }
        case RISA_VAL_INT: {
            return BYTE_VALUE((uint8_t) AS_INT(args[0]));
        }
        case RISA_VAL_FLOAT: {
            return BYTE_VALUE((uint8_t) AS_FLOAT(args[0]));
        }
        case RISA_VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case RISA_DVAL_STRING: {
                    return value_byte_from_string(AS_STRING(args[0])->chars, AS_STRING(args[0])->length);
                }
                case RISA_DVAL_ARRAY:
                case RISA_DVAL_OBJECT:
                case RISA_DVAL_UPVALUE:
                case RISA_DVAL_FUNCTION:
                case RISA_DVAL_CLOSURE:
                case RISA_DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static RisaValue risa_std_core_to_float(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case RISA_VAL_NULL: {
            return NULL_VALUE;
        }
        case RISA_VAL_BOOL: {
            return AS_BOOL(args[0]) ? FLOAT_VALUE(1) : FLOAT_VALUE(0);
        }
        case RISA_VAL_BYTE: {
            return FLOAT_VALUE((double) AS_BYTE(args[0]));
        }
        case RISA_VAL_INT: {
            return FLOAT_VALUE((double) AS_INT(args[0]));
        }
        case RISA_VAL_FLOAT: {
            return args[0];
        }
        case RISA_VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case RISA_DVAL_STRING: {
                    return value_float_from_string(AS_STRING(args[0])->chars);
                }
                case RISA_DVAL_ARRAY:
                case RISA_DVAL_OBJECT:
                case RISA_DVAL_UPVALUE:
                case RISA_DVAL_FUNCTION:
                case RISA_DVAL_CLOSURE:
                case RISA_DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static RisaValue risa_std_core_to_bool(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return NULL_VALUE;

    return BOOL_VALUE(!value_is_falsy(args[0]));
}

static RisaValue risa_std_core_foreach(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 2)
        return NULL_VALUE;

    if(!value_is_dense_of_type(args[0], RISA_DVAL_ARRAY))
        return NULL_VALUE;

    switch(args[1].type) {
        case RISA_VAL_DENSE:
            switch(AS_DENSE(args[1])->type) {
                case RISA_DVAL_FUNCTION:
                case RISA_DVAL_CLOSURE:
                case RISA_DVAL_NATIVE:
                    goto _std_core_foreach_work;
            } // Fallthrough
        default:
            return NULL_VALUE;
    }

_std_core_foreach_work: ;

    RisaDenseArray* array = AS_ARRAY(args[0]);

    for(size_t i = 0; i < array->data.size; ++i) {
        risa_vm_invoke(vm, args + argc, args[1], 1, array->data.values[i]);
    }

    return NULL_VALUE;
}

static RisaValue risa_std_core_typeof_internal(RisaVM* vm, RisaValue val) {
    #define TYPEOF_RESULT(type) DENSE_VALUE(risa_vm_string_create(vm, type, sizeof(type) - 1))

    switch(val.type) {
        case RISA_VAL_NULL:  return TYPEOF_RESULT("null");
        case RISA_VAL_BOOL:  return TYPEOF_RESULT("bool");
        case RISA_VAL_BYTE:  return TYPEOF_RESULT("byte");
        case RISA_VAL_INT:   return TYPEOF_RESULT("int");
        case RISA_VAL_FLOAT: return TYPEOF_RESULT("float");

        case RISA_VAL_DENSE: {
            switch(AS_DENSE(val)->type) {
                case RISA_DVAL_STRING:   return TYPEOF_RESULT("string");
                case RISA_DVAL_ARRAY:    return TYPEOF_RESULT("array");
                case RISA_DVAL_OBJECT:   return TYPEOF_RESULT("object");
                case RISA_DVAL_UPVALUE:  return risa_std_core_typeof_internal(vm, *((RisaDenseUpvalue *) (AS_DENSE(val)))->ref);
                case RISA_DVAL_FUNCTION:
                case RISA_DVAL_CLOSURE:
                case RISA_DVAL_NATIVE:   return TYPEOF_RESULT("function");
            }
        }

        default: return NULL_VALUE;
    }

    #undef TYPEOF_RESULT
}