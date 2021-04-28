#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

static Value std_debug_type_internal(VM* vm, Value val);

static Value std_debug_type(void* vm, uint8_t argc, Value* args) {
    if(argc > 0) {
        return std_debug_type_internal((VM*) vm, args[0]);
    }

    return NULL_VALUE;
}

static Value std_debug_vm_acc(void* vm, uint8_t argc, Value* args) {
    if(argc == 0) {
        return ((VM*) vm)->acc;
    }

    return (((VM*) vm)->acc = args[0]);
}

static Value std_debug_vm_heap_size(void* vm, uint8_t argc, Value* args) {
    return (INT_VALUE((uint64_t) ((VM*) vm)->heapSize));
}

static Value std_debug_vm_stack_size(void* vm, uint8_t argc, Value* args) {
    return (INT_VALUE((uint64_t) (VM_STACK_SIZE * sizeof(Value))));
}

void std_register_debug(VM* vm) {
    #define STD_DEBUG_ENTRY(name, fn) , RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, dense_native_value(std_debug_##fn)

    DenseObject* objVm = dense_object_create_with(vm, 3
                                                  STD_DEBUG_ENTRY(acc, vm_acc)
                                                  STD_DEBUG_ENTRY(heapSize, vm_heap_size)
                                                  STD_DEBUG_ENTRY(stackSize, vm_stack_size));

    DenseObject* obj = dense_object_create_with(vm, 2,
                                                "vm", sizeof("vm") - 1, DENSE_VALUE((DenseValue *) objVm)
                                                STD_DEBUG_ENTRY(type, type));

    vm_global_set(vm, "debug", sizeof("debug") - 1, DENSE_VALUE((DenseValue*) obj));

    #undef STD_DEBUG_ENTRY
}

static Value std_debug_type_internal(VM* vm, Value val) {
    #define TYPE_RESULT(type) DENSE_VALUE(vm_string_create(vm, type, sizeof(type) - 1))

    switch(val.type) {
        case VAL_NULL:  return TYPE_RESULT("null");
        case VAL_BOOL:  return TYPE_RESULT("bool");
        case VAL_BYTE:  return TYPE_RESULT("byte");
        case VAL_INT:   return TYPE_RESULT("int");
        case VAL_FLOAT: return TYPE_RESULT("float");

        case VAL_DENSE: {
            switch(AS_DENSE(val)->type) {
                case DVAL_STRING:   return TYPE_RESULT("string");
                case DVAL_ARRAY:    return TYPE_RESULT("array");
                case DVAL_OBJECT:   return TYPE_RESULT("object");
                case DVAL_UPVALUE:  return TYPE_RESULT("upvalue");
                case DVAL_FUNCTION: return TYPE_RESULT("function");
                case DVAL_CLOSURE:  return TYPE_RESULT("closure");
                case DVAL_NATIVE:   return TYPE_RESULT("native");
            }
        }

        default: return NULL_VALUE;
    }

    #undef TYPE_RESULT
}