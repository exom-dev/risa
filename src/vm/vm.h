#ifndef RISA_VM_H_GUARD
#define RISA_VM_H_GUARD

#include "../chunk/chunk.h"
#include "../common/def.h"
#include "../data/map.h"
#include "../value/dense.h"
#include "../options/options.h"

#define VM_STACK_SIZE CALLFRAME_STACK_SIZE * 251

typedef enum {
    FRAME_FUNCTION,
    FRAME_CLOSURE
} CallFrameType;

typedef struct {
    CallFrameType type;

    union {
        DenseFunction* function;
        DenseClosure* closure;
    } callee;

    uint8_t* ip;

    Value* base;
    Value* regs;
} CallFrame;

#define FRAME_FUNCTION(frame) ((frame).type == FRAME_FUNCTION ? (frame).callee.function : (frame).callee.closure->function)

typedef struct {
    CallFrame frames[CALLFRAME_STACK_SIZE];
    uint32_t frameCount;

    Value  stack[VM_STACK_SIZE];
    Value* stackTop;

    Map strings;
    Map globals;

    DenseValue* values;
    DenseUpvalue* upvalues;

    Options options;

    Value acc; // The accumulator, used for REPL mode to store the lastReg value.

    size_t heapSize;
    size_t heapThreshold;
} VM;

typedef enum {
    VM_OK,
    VM_ERROR
} VMStatus;

void vm_init(VM* vm);
void vm_clean(VM* vm);
void vm_delete(VM* vm);

VMStatus vm_execute(VM* vm);
VMStatus vm_run(VM* vm);

void vm_register_string(VM* vm, DenseString* string);
void vm_register_dense(VM* vm, DenseValue* dense);

DenseString* vm_string_internalize(VM* vm, const char* str, uint32_t length);

void vm_global_set(VM* vm, const char* str, uint32_t length, Value value);
void vm_global_set_native(VM* vm, const char* str, uint32_t length, NativeFunction fn);

void  vm_stack_reset(VM* vm);
void  vm_stack_push(VM* vm, Value value);
Value vm_stack_pop(VM* vm);
Value vm_stack_peek(VM* vm, size_t range);

#endif
