#include "vm.h"

static void vm_frame_base(VM* vm, CallFrame* frame, Value* base) {
    if(base == NULL)
        frame->base = vm->stackTop++;
    else frame->base = base;

    frame->regs = frame->base + 1;
}

CallFrame vm_frame_from_function(VM* vm, Value* base, DenseFunction* function, bool isolated) {
    CallFrame frame;

    frame.type = FRAME_FUNCTION;
    frame.callee.function = function;
    frame.ip = frame.callee.function->chunk.bytecode;
    frame.isolated = isolated;

    vm_frame_base(vm, &frame, base);

    return frame;
}

CallFrame vm_frame_from_closure(VM* vm, Value* base, DenseClosure* closure, bool isolated) {
    CallFrame frame;

    frame.type = FRAME_CLOSURE;
    frame.callee.closure = closure;
    frame.ip = closure->function->chunk.bytecode;
    frame.isolated = isolated;

    vm_frame_base(vm, &frame, base);

    return frame;
}