#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

#include <string.h>

static Value std_core_typeof_internal(VM* vm, Value val);

static Value std_core_typeof(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    return std_core_typeof_internal((VM*) vm, args[0]);
}

static Value std_core_to_string(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    char* str = value_to_string(args[0]);

    DenseString* result = vm_string_create(vm, str, strlen(str));

    RISA_MEM_FREE(str);

    return DENSE_VALUE((DenseValue*) result);
}

static Value std_core_to_int(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case VAL_NULL: {
            return NULL_VALUE;
        }
        case VAL_BOOL: {
            return AS_BOOL(args[0]) ? INT_VALUE(1) : INT_VALUE(0);
        }
        case VAL_BYTE: {
            return INT_VALUE((int64_t) AS_BYTE(args[0]));
        }
        case VAL_INT: {
            return args[0];
        }
        case VAL_FLOAT: {
            return INT_VALUE((int64_t) AS_FLOAT(args[0]));
        }
        case VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case DVAL_STRING: {
                    return value_int_from_string(AS_STRING(args[0])->chars, AS_STRING(args[0])->length);
                }
                case DVAL_ARRAY:
                case DVAL_OBJECT:
                case DVAL_UPVALUE:
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static Value std_core_to_byte(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case VAL_NULL: {
            return NULL_VALUE;
        }
        case VAL_BOOL: {
            return AS_BOOL(args[0]) ? BYTE_VALUE(1) : BYTE_VALUE(0);
        }
        case VAL_BYTE: {
            return args[0];
        }
        case VAL_INT: {
            return BYTE_VALUE((uint8_t) AS_INT(args[0]));
        }
        case VAL_FLOAT: {
            return BYTE_VALUE((uint8_t) AS_FLOAT(args[0]));
        }
        case VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case DVAL_STRING: {
                    return value_byte_from_string(AS_STRING(args[0])->chars, AS_STRING(args[0])->length);
                }
                case DVAL_ARRAY:
                case DVAL_OBJECT:
                case DVAL_UPVALUE:
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static Value std_core_to_float(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    switch(args[0].type) {
        case VAL_NULL: {
            return NULL_VALUE;
        }
        case VAL_BOOL: {
            return AS_BOOL(args[0]) ? FLOAT_VALUE(1) : FLOAT_VALUE(0);
        }
        case VAL_BYTE: {
            return FLOAT_VALUE((double) AS_BYTE(args[0]));
        }
        case VAL_INT: {
            return FLOAT_VALUE((double) AS_INT(args[0]));
        }
        case VAL_FLOAT: {
            return args[0];
        }
        case VAL_DENSE: {
            switch(AS_DENSE(args[0])->type) {
                case DVAL_STRING: {
                    return value_float_from_string(AS_STRING(args[0])->chars);
                }
                case DVAL_ARRAY:
                case DVAL_OBJECT:
                case DVAL_UPVALUE:
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:
                    return NULL_VALUE;
            }
        }
    }
}

static Value std_core_to_bool(void* vm, uint8_t argc, Value* args) {
    if(argc == 0)
        return NULL_VALUE;

    return BOOL_VALUE(!value_is_falsy(args[0]));
}

static Value std_core_foreach(void* vm, uint8_t argc, Value* args) {
    if(argc < 2)
        return NULL_VALUE;

    if(!value_is_dense_of_type(args[0], DVAL_ARRAY))
        return NULL_VALUE;

    switch(args[1].type) {
        case VAL_DENSE:
            switch(AS_DENSE(args[1])->type) {
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:
                    goto _std_core_foreach_work;
            } // Fallthrough
        default:
            return NULL_VALUE;
    }

_std_core_foreach_work: ;

    DenseArray* array = AS_ARRAY(args[0]);

    for(size_t i = 0; i < array->data.size; ++i) {
        vm_invoke(vm, args + argc, args[1], 1, array->data.values[i]);
    }

    return NULL_VALUE;
}

void std_register_core(VM* vm) {
    #define STD_CORE_ENTRY(name, fn) RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, std_core_##fn

    vm_global_set_native(vm, STD_CORE_ENTRY(typeof, typeof));
    vm_global_set_native(vm, STD_CORE_ENTRY(toString, to_string));
    vm_global_set_native(vm, STD_CORE_ENTRY(toInt, to_int));
    vm_global_set_native(vm, STD_CORE_ENTRY(toByte, to_byte));
    vm_global_set_native(vm, STD_CORE_ENTRY(toFloat, to_float));
    vm_global_set_native(vm, STD_CORE_ENTRY(toBool, to_bool));
    vm_global_set_native(vm, STD_CORE_ENTRY(foreach, foreach));

    #undef STD_CORE_ENTRY
}

static Value std_core_typeof_internal(VM* vm, Value val) {
    #define TYPEOF_RESULT(type) DENSE_VALUE(vm_string_create(vm, type, sizeof(type) - 1))

    switch(val.type) {
        case VAL_NULL:  return TYPEOF_RESULT("null");
        case VAL_BOOL:  return TYPEOF_RESULT("bool");
        case VAL_BYTE:  return TYPEOF_RESULT("byte");
        case VAL_INT:   return TYPEOF_RESULT("int");
        case VAL_FLOAT: return TYPEOF_RESULT("float");

        case VAL_DENSE: {
            switch(AS_DENSE(val)->type) {
                case DVAL_STRING:   return TYPEOF_RESULT("string");
                case DVAL_ARRAY:    return TYPEOF_RESULT("array");
                case DVAL_OBJECT:   return TYPEOF_RESULT("object");
                case DVAL_UPVALUE:  return std_core_typeof_internal(vm, *((DenseUpvalue*) (AS_DENSE(val)))->ref);
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:   return TYPEOF_RESULT("function");
            }
        }

        default: return NULL_VALUE;
    }

    #undef TYPEOF_RESULT
}