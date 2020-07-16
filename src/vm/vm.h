#ifndef RISA_VM_H_GUARD
#define RISA_VM_H_GUARD

#include "../chunk/chunk.h"
#include "../common/def.h"
#include "../data/map.h"

#define VM_STACK_SIZE CALLFRAME_STACK_SIZE * 251

typedef struct {
    DenseFunction* function;

    uint8_t* ip;

    Value* base;
    Value* regs;
} CallFrame;

typedef struct {
    CallFrame frames[CALLFRAME_STACK_SIZE];
    uint32_t frameCount;

    Value  stack[VM_STACK_SIZE];
    Value* stackTop;

    Map strings;
    Map globals;

    DenseValue* values;
} VM;

typedef enum {
    VM_OK,
    VM_ERROR
} VMStatus;

void vm_init(VM* vm);
void vm_delete(VM* vm);

VMStatus vm_execute(VM* vm);
VMStatus vm_run(VM* vm);

void vm_register_string(VM* vm, DenseString* string);
void vm_register_value(VM* vm, DenseValue* value);

void  vm_stack_reset(VM* vm);
void  vm_stack_push(VM* vm, Value value);
Value vm_stack_pop(VM* vm);
Value vm_stack_peek(VM* vm, size_t range);

#endif
