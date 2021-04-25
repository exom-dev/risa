#include "std.h"
#include "../value/value.h"

static Value std_core_internal_typeof(VM* vm, Value val) {
    switch(val.type) {
        case VAL_NULL:  return DENSE_VALUE(vm_string_internalize(vm, "null", sizeof("null") - 1));
        case VAL_BOOL:  return DENSE_VALUE(vm_string_internalize(vm, "bool", sizeof("bool") - 1));
        case VAL_BYTE:  return DENSE_VALUE(vm_string_internalize(vm, "byte", sizeof("byte") - 1));
        case VAL_INT:   return DENSE_VALUE(vm_string_internalize(vm, "int", sizeof("int") - 1));
        case VAL_FLOAT: return DENSE_VALUE(vm_string_internalize(vm, "float", sizeof("float") - 1));
        case VAL_DENSE: {
            switch(AS_DENSE(val)->type) {
                case DVAL_STRING:   return DENSE_VALUE(vm_string_internalize(vm, "string", sizeof("string") - 1));
                case DVAL_ARRAY:    return DENSE_VALUE(vm_string_internalize(vm, "array", sizeof("array") - 1));
                case DVAL_OBJECT:   return DENSE_VALUE(vm_string_internalize(vm, "object", sizeof("object") - 1));
                case DVAL_UPVALUE:  return std_core_internal_typeof(vm, *((DenseUpvalue*) (AS_DENSE(val)))->ref);
                case DVAL_FUNCTION:
                case DVAL_CLOSURE:
                case DVAL_NATIVE:   return DENSE_VALUE(vm_string_internalize(vm, "function", sizeof("function") - 1));
            }
        }
    }
}

static Value std_core_typeof(void* vm, uint8_t argc, Value* args) {
    if(argc > 0) {
        return std_core_internal_typeof((VM*) vm, args[0]);
    }

    return NULL_VALUE;
}

void std_register_core(VM* vm) {
    vm_global_set_native(vm, "typeof", sizeof("typeof") - 1, std_core_typeof);
}
