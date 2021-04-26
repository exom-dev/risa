#include "std.h"
#include "../value/value.h"

static Value std_core_typeof_internal(VM* vm, Value val);

static Value std_core_typeof(void* vm, uint8_t argc, Value* args) {
    if(argc > 0) {
        return std_core_typeof_internal((VM*) vm, args[0]);
    }

    return NULL_VALUE;
}

void std_register_core(VM* vm) {
    vm_global_set_native(vm, "typeof", sizeof("typeof") - 1, std_core_typeof);
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