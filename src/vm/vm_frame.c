#include "vm.h"

static void vm_frame_base(RisaVM* vm, RisaCallFrame* frame, RisaValue* base) {
    if(base == NULL)
        frame->base = vm->stackTop++;
    else frame->base = base;

    frame->regs = frame->base + 1;
}

RisaCallFrame risa_vm_frame_from_function(RisaVM* vm, RisaValue* base, RisaDenseFunction* function, bool isolated) {
    RisaCallFrame frame;

    frame.type = RISA_FRAME_FUNCTION;
    frame.callee.function = function;
    frame.ip = frame.callee.function->cluster.bytecode;
    frame.isolated = isolated;

    vm_frame_base(vm, &frame, base);

    return frame;
}

RisaCallFrame risa_vm_frame_from_closure(RisaVM* vm, RisaValue* base, RisaDenseClosure* closure, bool isolated) {
    RisaCallFrame frame;

    frame.type = RISA_FRAME_CLOSURE;
    frame.callee.closure = closure;
    frame.ip = closure->function->cluster.bytecode;
    frame.isolated = isolated;

    vm_frame_base(vm, &frame, base);

    return frame;
}