#include "vm.h"

#include "../memory/mem.h"
#include "../chunk/bytecode.h"
#include "../debug/disassembler.h"

#include "../common/logging.h"

VM* vm_create() {
    VM* vm = mem_alloc(sizeof(VM));
    vm_init(vm);

    return vm;
}

void vm_init(VM* vm) {
    vm->ip = 0;
    vm->chunk = NULL;
    vm_stack_reset(vm);
}

void vm_delete(VM* vm) {

}

VMStatus vm_execute(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = vm->chunk->bytecode;

    return vm_run(vm);
}

VMStatus vm_run(VM* vm) {
    #define NEXT_BYTE() (*vm->ip++)
    #define NEXT_CONSTANT() (vm->chunk->constants.values[NEXT_BYTE()])

    while(1) {
        #ifdef DEBUG_TRACE_EXECUTION
            debug_disassemble_instruction(vm->chunk, (size_t) (vm->ip - vm->chunk->bytecode));

            PRINT("          ");
            for(Value* entry = vm->stack; entry < vm->stackTop; ++entry) {
                PRINT("[ ");
                value_print(*entry);
                PRINT(" ]");
            }
            PRINT("\n");
        #endif

        uint8_t instruction = NEXT_BYTE();

        switch(instruction) {
            case OP_CNST: {
                Value constant = NEXT_CONSTANT();

                vm_stack_push(vm, constant);
                break;
            }
            case OP_ADD: {
                double right = vm_stack_pop(vm);
                double left = vm_stack_pop(vm);

                vm_stack_push(vm, left + right);
                break;
            }
            case OP_SUB: {
                double right = vm_stack_pop(vm);
                double left = vm_stack_pop(vm);

                vm_stack_push(vm, left - right);
                break;
            }
            case OP_MUL: {
                double right = vm_stack_pop(vm);
                double left = vm_stack_pop(vm);

                vm_stack_push(vm, left * right);
                break;
            }
            case OP_DIV: {
                double right = vm_stack_pop(vm);
                double left = vm_stack_pop(vm);

                vm_stack_push(vm, left / right);
                break;
            }
            case OP_NEG: {
                vm_stack_push(vm, -vm_stack_pop(vm));
                break;
            }
            case OP_RET: {
                value_print(vm_stack_pop(vm));
                PRINT("\n");
                return VM_OK;
            }
            default: {

            }
        }
    }

    #undef NEXT_CONSTANT
    #undef NEXT_BYTE
}