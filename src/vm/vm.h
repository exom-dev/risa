#ifndef RISA_VM_H_GUARD
#define RISA_VM_H_GUARD

#include "../chunk/chunk.h"
#include "../common/def.h"

#define VM_STACK_SIZE 256

typedef struct {
    Chunk* chunk;

    uint8_t* ip;

    Value  stack[VM_STACK_SIZE];
    Value* stackTop;
} VM;

typedef enum {
    VM_OK,
    VM_ERROR
} VMStatus;

VM*  vm_create();
void vm_init(VM* vm);
void vm_delete(VM* vm);

VMStatus vm_execute(VM* vm, Chunk* chunk);
VMStatus vm_run(VM* vm);

void  vm_stack_reset(VM* vm);
void  vm_stack_push(VM* vm, Value value);
Value vm_stack_pop(VM* vm);
Value vm_stack_peek(VM* vm, size_t range);

#endif
