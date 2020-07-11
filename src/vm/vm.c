#include "vm.h"

#include "../memory/mem.h"
#include "../chunk/bytecode.h"
#include "../debug/disassembler.h"

#include "../common/logging.h"

void vm_init(VM* vm) {
    vm->ip = 0;
    vm->chunk = NULL;
    vm_stack_reset(vm);

    vm->regs = vm->stackTop;
}

void vm_delete(VM* vm) {

}

VMStatus vm_execute(VM* vm) {
    return vm_run(vm);
}

VMStatus vm_run(VM* vm) {
    #define NEXT_BYTE() (*vm->ip++)
    #define NEXT_CONSTANT() (vm->chunk->constants.values[NEXT_BYTE()])

    #define DEST  (*vm->ip)
    #define LEFT  (vm->ip[1])
    #define RIGHT (vm->ip[2])

    #define LEFT_CONSTANT (vm->chunk->constants.values[LEFT])

    #define DEST_REG  (vm->regs[DEST])
    #define LEFT_REG  (vm->regs[LEFT])
    #define RIGHT_REG (vm->regs[RIGHT])

    #define SKIP_ARGS(count) (vm->ip += count)

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
                DEST_REG = LEFT_CONSTANT;

                SKIP_ARGS(2);
                break;
            }
            case OP_ADD: {
                DEST_REG = LEFT_REG + RIGHT_REG;

                SKIP_ARGS(3);
                break;
            }
            case OP_SUB: {
                DEST_REG = LEFT_REG - RIGHT_REG;

                SKIP_ARGS(3);
                break;
            }
            case OP_MUL: {
                DEST_REG = LEFT_REG * RIGHT_REG;

                SKIP_ARGS(3);
                break;
            }
            case OP_DIV: {
                DEST_REG = LEFT_REG / RIGHT_REG;

                SKIP_ARGS(3);
                break;
            }
            case OP_NEG: {
                DEST_REG = -LEFT_REG;

                SKIP_ARGS(2);
                break;
            }
            case OP_RET: {
                value_print(*vm->regs);
                PRINT("\n");
                return VM_OK;
            }
            default: {

            }
        }
    }

    #undef RIGHT_REG
    #undef LEFT_REG
    #undef DEST_REG

    #undef LEFT_CONSTANT

    #undef RIGHT
    #undef LEFT
    #undef DEST

    #undef NEXT_CONSTANT
    #undef NEXT_BYTE
}