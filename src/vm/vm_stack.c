#include "vm.h"

void risa_vm_stack_reset(RisaVM* vm) {
    vm->frameCount = 0;
    vm->stackTop = vm->stack;
    vm->upvalues = NULL;
}

void risa_vm_stack_push(RisaVM* vm, RisaValue value) {
    *vm->stackTop = value;
    ++vm->stackTop;
}

RisaValue risa_vm_stack_pop(RisaVM* vm) {
    --vm->stackTop;
    return *vm->stackTop;
}

RisaValue risa_vm_stack_peek(RisaVM* vm, size_t range) {
    return *(vm->stackTop - range);
}
