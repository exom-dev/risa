#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"
#include "../mem/gc.h"

#include <stdio.h>

static RisaValue risa_std_debug_type          (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_addr          (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_acc        (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_heap_size  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_stack_size (void*, uint8_t, RisaValue*);
static RisaValue risa_std_debug_vm_gc         (void*, uint8_t, RisaValue*);

static RisaValue risa_std_debug_type_internal (RisaVM*, RisaValue);

void risa_std_register_debug(RisaVM* vm) {
    #define STD_DEBUG_OBJ_ENTRY(name, fn) , RISA_STRINGIFY(name), sizeof(RISA_STRINGIFY(name)) - 1, risa_dense_native_value(risa_std_debug_##fn)

    RisaDenseObject* objVm = risa_dense_object_create_under(vm, 4
                                                            STD_DEBUG_OBJ_ENTRY(acc, vm_acc)
                                                            STD_DEBUG_OBJ_ENTRY(heapSize, vm_heap_size)
                                                            STD_DEBUG_OBJ_ENTRY(stackSize, vm_stack_size)
                                                            STD_DEBUG_OBJ_ENTRY(gc, vm_gc));

    RisaDenseObject* obj = risa_dense_object_create_under(vm, 3,
                                                          "vm", sizeof("vm") - 1, risa_value_from_dense((RisaDenseValue *) objVm)
                                                          STD_DEBUG_OBJ_ENTRY(type, type)
                                                          STD_DEBUG_OBJ_ENTRY(addr, addr));

    risa_vm_global_set(vm, "debug", sizeof("debug") - 1, risa_value_from_dense((RisaDenseValue *) obj));

    #undef STD_DEBUG_OBJ_ENTRY
}

static RisaValue risa_std_debug_type(void* vm, uint8_t argc, RisaValue* args) {
    if(argc > 0) {
        return risa_std_debug_type_internal((RisaVM *) vm, args[0]);
    }

    return risa_value_from_null();
}

static RisaValue risa_std_debug_addr(void* vm, uint8_t argc, RisaValue* args) {
    if(argc > 0) {
        if(!value_is_dense(args[0])) {
            return risa_value_from_null();
        }

        size_t size = snprintf(NULL, 0, "%p", risa_value_as_dense(args[0]));
        char* buffer = RISA_MEM_ALLOC(size + 1);

        snprintf(buffer, size + 1, "%p", risa_value_as_dense(args[0]));

        RisaValue addr = risa_value_from_dense((RisaDenseValue*) risa_vm_string_create(vm, buffer, size));
        RISA_MEM_FREE(buffer);

        return addr;
    }

    return risa_value_from_null();
}

static RisaValue risa_std_debug_vm_acc(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0) {
        return ((RisaVM*) vm)->acc;
    }

    return (((RisaVM*) vm)->acc = args[0]);
}

static RisaValue risa_std_debug_vm_heap_size(void* vm, uint8_t argc, RisaValue* args) {
    return (risa_value_from_int((uint64_t) ((RisaVM*) vm)->heapSize));
}

static RisaValue risa_std_debug_vm_stack_size(void* vm, uint8_t argc, RisaValue* args) {
    return (risa_value_from_int((uint64_t) (RISA_VM_STACK_SIZE * sizeof(RisaValue))));
}

static RisaValue risa_std_debug_vm_gc(void* vm, uint8_t argc, RisaValue* args) {
    risa_gc_run((RisaVM*) vm);

    return risa_value_from_null();
}

static RisaValue risa_std_debug_type_internal(RisaVM* vm, RisaValue val) {
    #define TYPE_RESULT(type) risa_value_from_dense((RisaDenseValue*) risa_vm_string_create(vm, type, sizeof(type) - 1))

    switch(val.type) {
        case RISA_VAL_NULL:  return TYPE_RESULT("null");
        case RISA_VAL_BOOL:  return TYPE_RESULT("bool");
        case RISA_VAL_BYTE:  return TYPE_RESULT("byte");
        case RISA_VAL_INT:   return TYPE_RESULT("int");
        case RISA_VAL_FLOAT: return TYPE_RESULT("float");

        case RISA_VAL_DENSE: {
            switch(risa_value_as_dense(val)->type) {
                case RISA_DVAL_STRING:   return TYPE_RESULT("string");
                case RISA_DVAL_ARRAY:    return TYPE_RESULT("array");
                case RISA_DVAL_OBJECT:   return TYPE_RESULT("object");
                case RISA_DVAL_UPVALUE:  return TYPE_RESULT("upvalue");
                case RISA_DVAL_FUNCTION: return TYPE_RESULT("function");
                case RISA_DVAL_CLOSURE:  return TYPE_RESULT("closure");
                case RISA_DVAL_NATIVE:   return TYPE_RESULT("native");
            }
        }

        default: return risa_value_from_null();
    }

    #undef TYPE_RESULT
}