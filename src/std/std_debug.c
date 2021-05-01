#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

static RisaValue risa_std_debug_type          (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_acc        (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_heap_size  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_stack_size (void*, uint8_t, RisaValue*);

static RisaValue risa_std_debug_type_internal (RisaVM*, RisaValue);

void risa_std_register_debug(RisaVM* vm) {
    #define STD_DEBUG_OBJ_ENTRY(name, fn) , RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, risa_dense_native_value(risa_std_debug_##fn)

    RisaDenseObject* objVm = risa_dense_object_create_under(vm, 3
                                                            STD_DEBUG_OBJ_ENTRY(acc, vm_acc)
                                                            STD_DEBUG_OBJ_ENTRY(heapSize, vm_heap_size)
                                                            STD_DEBUG_OBJ_ENTRY(stackSize, vm_stack_size));

    RisaDenseObject* obj = risa_dense_object_create_under(vm, 2,
                                                          "vm", sizeof("vm") - 1, RISA_DENSE_VALUE((RisaDenseValue *) objVm)
                                                          STD_DEBUG_OBJ_ENTRY(type, type));

    risa_vm_global_set(vm, "debug", sizeof("debug") - 1, RISA_DENSE_VALUE((RisaDenseValue *) obj));

    #undef STD_DEBUG_OBJ_ENTRY
}

static RisaValue risa_std_debug_type(void* vm, uint8_t argc, RisaValue* args) {
    if(argc > 0) {
        return risa_std_debug_type_internal((RisaVM *) vm, args[0]);
    }

    return RISA_NULL_VALUE;
}

static RisaValue risa_std_debug_vm_acc(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0) {
        return ((RisaVM*) vm)->acc;
    }

    return (((RisaVM*) vm)->acc = args[0]);
}

static RisaValue risa_std_debug_vm_heap_size(void* vm, uint8_t argc, RisaValue* args) {
    return (RISA_INT_VALUE((uint64_t) ((RisaVM*) vm)->heapSize));
}

static RisaValue risa_std_debug_vm_stack_size(void* vm, uint8_t argc, RisaValue* args) {
    return (RISA_INT_VALUE((uint64_t) (RISA_VM_STACK_SIZE * sizeof(RisaValue))));
}

static RisaValue risa_std_debug_type_internal(RisaVM* vm, RisaValue val) {
    #define TYPE_RESULT(type) RISA_DENSE_VALUE(risa_vm_string_create(vm, type, sizeof(type) - 1))

    switch(val.type) {
        case RISA_VAL_NULL:  return TYPE_RESULT("null");
        case RISA_VAL_BOOL:  return TYPE_RESULT("bool");
        case RISA_VAL_BYTE:  return TYPE_RESULT("byte");
        case RISA_VAL_INT:   return TYPE_RESULT("int");
        case RISA_VAL_FLOAT: return TYPE_RESULT("float");

        case RISA_VAL_DENSE: {
            switch(RISA_AS_DENSE(val)->type) {
                case RISA_DVAL_STRING:   return TYPE_RESULT("string");
                case RISA_DVAL_ARRAY:    return TYPE_RESULT("array");
                case RISA_DVAL_OBJECT:   return TYPE_RESULT("object");
                case RISA_DVAL_UPVALUE:  return TYPE_RESULT("upvalue");
                case RISA_DVAL_FUNCTION: return TYPE_RESULT("function");
                case RISA_DVAL_CLOSURE:  return TYPE_RESULT("closure");
                case RISA_DVAL_NATIVE:   return TYPE_RESULT("native");
            }
        }

        default: return RISA_NULL_VALUE;
    }

    #undef TYPE_RESULT
}