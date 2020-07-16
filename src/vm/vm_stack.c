#include "vm.h"

void  vm_stack_reset(VM* vm) {
    vm->frameCount = 0;
    vm->stackTop = vm->stack;
}

void  vm_stack_push(VM* vm, Value value) {
    *vm->stackTop = value;
    ++vm->stackTop;
}

Value vm_stack_pop(VM* vm) {
    --vm->stackTop;
    return *vm->stackTop;
}

Value vm_stack_peek(VM* vm, size_t range) {
    return *(vm->stackTop - range);
}
