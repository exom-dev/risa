#include "vm.h"

#include "../memory/mem.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../value/dense.h"

#ifdef DEBUG_TRACE_EXECUTION
    #include "../debug/disassembler.h"
#endif

#define VM_RUNTIME_ERROR(vm, fmt, ...) \
    fprintf(stderr, "[error] at index %u: " fmt "\n", vm->frames[vm->frameCount - 1].function->chunk.indices[vm->frames[vm->frameCount - 1].ip - vm->frames[vm->frameCount - 1].function->chunk.bytecode], ##__VA_ARGS__ )

static bool call_value(VM* vm, uint8_t reg, uint8_t argc);
static bool call_function(VM* vm, uint8_t reg, uint8_t argc);

void vm_init(VM* vm) {
    vm_stack_reset(vm);

    map_init(&vm->strings);
    map_init(&vm->globals);

    vm->values = NULL;
}

void vm_delete(VM* vm) {
    map_delete(&vm->strings);
    map_delete(&vm->globals);

    DenseValue* value = vm->values;

    while(value != NULL) {
        DenseValue* next = value->next;
        switch(value->type) {
            case DVAL_STRING:
                MEM_FREE(value);
                break;
            case DVAL_FUNCTION:
                chunk_delete(&((DenseFunction*) value)->chunk);
                break;
        }

        value = next;
    }
}

VMStatus vm_execute(VM* vm) {
    return vm_run(vm);
}

VMStatus vm_run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    #define NEXT_BYTE() (*frame->ip++)
    #define NEXT_CONSTANT() (frame->function->chunk.constants.values[NEXT_BYTE()])

    #define DEST     (*frame->ip)
    #define LEFT     (frame->ip[1])
    #define RIGHT    (frame->ip[2])
    #define COMBINED ((((uint16_t) frame->ip[1]) << 8) | (frame->ip[2]))

    #define DEST_CONSTANT     (frame->function->chunk.constants.values[DEST])
    #define LEFT_CONSTANT     (frame->function->chunk.constants.values[LEFT])
    #define RIGHT_CONSTANT    (frame->function->chunk.constants.values[RIGHT])
    #define COMBINED_CONSTANT (frame->function->chunk.constants.values[COMBINED])

    #define DEST_REG  (frame->regs[DEST])
    #define LEFT_REG  (frame->regs[LEFT])
    #define RIGHT_REG (frame->regs[RIGHT])

    #define SKIP(count) (frame->ip += count)
    #define BSKIP(count) (frame->ip -= count)

    while(1) {
        #ifdef DEBUG_TRACE_EXECUTION
            debug_disassemble_instruction(&frame->function->chunk, (uint32_t) (frame->ip - frame->function->chunk.bytecode));

            PRINT("          ");
            for(Value* entry = vm->stack; entry < vm->stackTop + 5; ++entry) {
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

                SKIP(3);
                break;
            }
            case OP_CNSTW: {
                DEST_REG = COMBINED_CONSTANT;

                SKIP(3);
                break;
            }
            case OP_MOV: {
                DEST_REG = LEFT_REG;

                SKIP(3);
                break;
            }
            case OP_DGLOB: {
                map_set(&vm->globals, AS_STRING(LEFT_CONSTANT), DEST_REG);

                SKIP(3);
                break;
            }
            case OP_GGLOB: {
                Value value;

                if(!map_get(&vm->globals, AS_STRING(LEFT_CONSTANT), &value)) {
                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(LEFT_CONSTANT));
                    return VM_ERROR;
                }

                DEST_REG = value;

                SKIP(3);
                break;
            }
            case OP_SGLOB: {
                if(map_set(&vm->globals, AS_STRING(DEST_CONSTANT), LEFT_REG)) {
                    map_erase(&vm->globals, AS_STRING(DEST_CONSTANT));

                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(DEST_CONSTANT));
                }

                SKIP(3);
                break;
            }
            case OP_NULL: {
                DEST_REG = NULL_VALUE;

                SKIP(3);
                break;
            }
            case OP_TRUE: {
                DEST_REG = BOOL_VALUE(true);

                SKIP(3);
                break;
            }
            case OP_FALSE: {
                DEST_REG = BOOL_VALUE(false);

                SKIP(3);
                break;
            }
            case OP_NOT: {
                DEST_REG = BOOL_VALUE(value_is_falsy(LEFT_REG));

                SKIP(3);
                break;
            }
            case OP_BNOT: {
                if(IS_BYTE(LEFT_REG))
                    DEST_REG = BYTE_VALUE(~AS_BYTE(LEFT_REG));
                else if(IS_INT(LEFT_REG))
                    DEST_REG = INT_VALUE(~AS_INT(LEFT_REG));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_NEG: {
                if(IS_BYTE(LEFT_REG))
                    DEST_REG = INT_VALUE(-((int64_t) AS_BYTE(LEFT_REG)));
                else if(IS_INT(LEFT_REG))
                    DEST_REG = INT_VALUE(-AS_INT(LEFT_REG));
                else if(IS_FLOAT(LEFT_REG))
                    DEST_REG = FLOAT_VALUE(-AS_FLOAT(LEFT_REG));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_ADD: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) + AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) + AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) + AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) + AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) + AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) + AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) + AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) + AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) + AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(value_is_dense_of_type(LEFT_REG, DVAL_STRING)) {
                    if(value_is_dense_of_type(RIGHT_REG, DVAL_STRING)) {
                        DenseString* result = dense_string_concat(AS_STRING(LEFT_REG), AS_STRING(RIGHT_REG));
                        DenseString* interned = map_find(&vm->strings, result->chars, result->length, result->hash);

                        if(interned != NULL) {
                            MEM_FREE(result);
                            result = interned;
                        } else vm_register_value(vm, (DenseValue*) result);

                        DEST_REG = DENSE_VALUE(result);
                    } else {
                        VM_RUNTIME_ERROR(vm, "Left operand must be string");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int, float or string");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_SUB: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) - AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) - AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) - AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) - AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) - AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) - AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) - AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) - AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) - AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_MUL: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) * AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) * AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) * AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) * AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) * AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) * AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) * AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) * AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) * AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_DIV: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) / AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) / AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) / AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) / AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) / AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) / AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) / AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) / AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) / AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_MOD: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) % AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) % AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_INT(LEFT_REG) % AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) % AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_SHL: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) << AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        if(AS_INT(RIGHT_REG) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) << AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(AS_INT(LEFT_REG) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return VM_ERROR;
                    }

                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) << AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        if(AS_INT(RIGHT_REG) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) << AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_SHR: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) >> AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        if(AS_INT(RIGHT_REG) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) >> AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(AS_INT(LEFT_REG) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return VM_ERROR;
                    }

                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) >> AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        if(AS_INT(RIGHT_REG) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) >> AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_GT: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) > AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) > AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) > AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) > AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) > AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) > AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) > AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) > AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) > AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_GTE: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_LT: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_LTE: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_INT(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_EQ: {
                DEST_REG = BOOL_VALUE(value_equals(LEFT_REG, RIGHT_REG));

                SKIP(3);
                break;
            }
            case OP_NEQ: {
                DEST_REG = BOOL_VALUE(!value_equals(LEFT_REG, RIGHT_REG));

                SKIP(3);
                break;
            }
            case OP_BAND: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) & AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) & AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) & AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) & AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_BXOR: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) ^ AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) ^ AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) ^ AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) ^ AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_BOR: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) | AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) | AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) | AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) | AS_INT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_TEST: {
                if(!value_is_falsy(DEST_REG))
                    SKIP(4);
                SKIP(3);
                break;
            }
            case OP_NTEST: {
                if(value_is_falsy(DEST_REG))
                    SKIP(4);
                SKIP(3);
                break;
            }
            case OP_JMP: {
                SKIP(DEST * 4);
                SKIP(3);
                break;
            }
            case OP_JMPW: {
                uint16_t amount = *((uint16_t*) &frame->ip);

                SKIP(amount * 4);
                SKIP(3);
                break;
            }
            case OP_BJMP: {
                BSKIP(DEST * 4);
                BSKIP(1);
                break;
            }
            case OP_BJMPW: {
                uint16_t amount = *((uint16_t*) &frame->ip);

                BSKIP(amount * 4);
                BSKIP(1);
                break;
            }
            case OP_CALL: {
                if(!call_value(vm, DEST, LEFT))
                    return VM_ERROR;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RET: {
                --vm->frameCount;

                if(vm->frameCount == 0)
                    return VM_OK;

                if(DEST == 251)
                    *frame->base = NULL_VALUE;
                else *frame->base = DEST_REG;

                vm->stackTop = frame->base + 1;
                frame = &vm->frames[vm->frameCount - 1];

                SKIP(3);
                break;
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

void vm_register_string(VM* vm, DenseString* string) {
    map_set(&vm->strings, string, NULL_VALUE);
}

void vm_register_value(VM* vm, DenseValue* value) {
    value->next = vm->values;
    vm->values = value;
}

static bool call_value(VM* vm, uint8_t reg, uint8_t argc) {
    Value value = vm->frames[vm->frameCount - 1].regs[reg];

    if(IS_DENSE(value)) {
        switch(AS_DENSE(value)->type) {
            case DVAL_FUNCTION:
                return call_function(vm, reg, argc);
            default: ;
        }
    }

    VM_RUNTIME_ERROR(vm, "Cannot call non-function type");
    return false;
}

static bool call_function(VM* vm, uint8_t reg, uint8_t argc) {
    DenseFunction* function = AS_FUNCTION(vm->frames[vm->frameCount - 1].regs[reg]);

    if(argc != function->arity) {
        VM_RUNTIME_ERROR(vm, "Expected %x args, got %x", function->arity, argc);
        return false;
    }

    if(vm->frameCount == CALLFRAME_STACK_SIZE) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.bytecode;

    frame->base = &vm->frames[vm->frameCount - 2].regs[reg];
    frame->regs = frame->base + 1;

    return true;
}

#undef VM_RUNTIME_ERROR