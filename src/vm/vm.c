#include "vm.h"

#include "../memory/mem.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"

#ifdef DEBUG_TRACE_EXECUTION
    #include "../debug/disassembler.h"
#endif

void vm_init(VM* vm) {
    vm->ip = 0;
    vm->chunk = NULL;
    vm_stack_reset(vm);

    vm->regs = vm->stackTop;

    map_init(&vm->strings);
    map_init(&vm->globals);

    vm->values = NULL;
}

void vm_delete(VM* vm) {
    map_delete(&vm->strings);
    map_delete(&vm->globals);

    LinkedValue* value = vm->values;

    while(value != NULL) {
        LinkedValue* next = value->next;
        MEM_FREE(value);

        value = next;
    }
}

VMStatus vm_execute(VM* vm) {
    return vm_run(vm);
}

VMStatus vm_run(VM* vm) {
    #define NEXT_BYTE() (*vm->ip++)
    #define NEXT_CONSTANT() (vm->chunk->constants.values[NEXT_BYTE()])

    #define DEST     (*vm->ip)
    #define LEFT     (vm->ip[1])
    #define RIGHT    (vm->ip[2])
    #define COMBINED ((((uint16_t) vm->ip[1]) << 8) | (vm->ip[2]))

    #define LEFT_CONSTANT     (vm->chunk->constants.values[LEFT])
    #define RIGHT_CONSTANT    (vm->chunk->constants.values[RIGHT])
    #define COMBINED_CONSTANT (vm->chunk->constants.values[COMBINED])

    #define DEST_REG  (vm->regs[DEST])
    #define LEFT_REG  (vm->regs[LEFT])
    #define RIGHT_REG (vm->regs[RIGHT])

    #define SKIP_ARGS(count) (vm->ip += count)

    #define VM_RUNTIME_ERROR(vm, fmt, ...) \
        fprintf(stderr, "[error] at index %u: " fmt "\n", vm->chunk->indices[vm->ip - vm->chunk->bytecode], ##__VA_ARGS__ )

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
            case OP_CNSTW: {
                DEST_REG = COMBINED_CONSTANT;

                SKIP_ARGS(3);
                break;
            }
            case OP_MOV: {
                DEST_REG = LEFT_REG;

                SKIP_ARGS(2);
                break;
            }
            case OP_DGLOB: {
                map_set(&vm->globals, AS_STRING(LEFT_CONSTANT), DEST_REG);

                SKIP_ARGS(2);
                break;
            }
            case OP_GGLOB: {
                Value value;

                if(!map_get(&vm->globals, AS_STRING(LEFT_CONSTANT), &value)) {
                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(LEFT_CONSTANT));
                    return VM_ERROR;
                }

                DEST_REG = value;

                SKIP_ARGS(2);
                break;
            }
            case OP_SGLOB: {
                if(map_set(&vm->globals, AS_STRING(LEFT_CONSTANT), DEST_REG)) {
                    map_erase(&vm->globals, AS_STRING(LEFT_CONSTANT));

                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(LEFT_CONSTANT));
                }

                SKIP_ARGS(2);
                break;
            }
            case OP_NULL: {
                DEST_REG = NULL_VALUE;

                SKIP_ARGS(1);
                break;
            }
            case OP_TRUE: {
                DEST_REG = BOOL_VALUE(true);

                SKIP_ARGS(1);
                break;
            }
            case OP_FALSE: {
                DEST_REG = BOOL_VALUE(false);

                SKIP_ARGS(1);
                break;
            }
            case OP_NOT: {
                DEST_REG = BOOL_VALUE(value_is_falsy(LEFT_REG));

                SKIP_ARGS(2);
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

                SKIP_ARGS(2);
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

                SKIP_ARGS(2);
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
                } else if(value_is_linked_of_type(LEFT_REG, LVAL_STRING)) {
                    if(value_is_linked_of_type(RIGHT_REG, LVAL_STRING)) {
                        ValString* result = value_string_concat(AS_STRING(LEFT_REG), AS_STRING(RIGHT_REG));
                        ValString* interned = map_find(&vm->strings, result->chars, result->length, result->hash);

                        if(interned != NULL) {
                            MEM_FREE(result);
                            result = interned;
                        } else vm_register_value(vm, (LinkedValue*) result);

                        DEST_REG = LINKED_VALUE(result);
                    } else {
                        VM_RUNTIME_ERROR(vm, "Left operand must be string");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int, float or string");
                    return VM_ERROR;
                }

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
                break;
            }
            case OP_GT: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) > AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) > AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) > AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) > AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) > AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) > AS_FLOAT(RIGHT_REG));
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

                SKIP_ARGS(3);
                break;
            }
            case OP_GTE: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) >= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) >= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) >= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP_ARGS(3);
                break;
            }
            case OP_LT: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) < AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) < AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) < AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP_ARGS(3);
                break;
            }
            case OP_LTE: {
                if(IS_BYTE(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_BYTE(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = INT_VALUE(AS_INT(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(LEFT_REG)) {
                    if(IS_BYTE(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) <= AS_BYTE(RIGHT_REG));
                    } else if(IS_INT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) <= AS_INT(RIGHT_REG));
                    } else if(IS_FLOAT(RIGHT_REG)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(LEFT_REG) <= AS_FLOAT(RIGHT_REG));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return VM_ERROR;
                }

                SKIP_ARGS(3);
                break;
            }
            case OP_EQ: {
                DEST_REG = BOOL_VALUE(value_equals(LEFT_REG, RIGHT_REG));

                SKIP_ARGS(3);
                break;
            }
            case OP_NEQ: {
                DEST_REG = BOOL_VALUE(!value_equals(LEFT_REG, RIGHT_REG));

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
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

                SKIP_ARGS(3);
                break;
            }
            case OP_RET: {
                return VM_OK;
            }
            default: {

            }
        }
    }

    #undef VM_RUNTIME_ERROR

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

void vm_register_string(VM* vm, ValString* string) {
    map_set(&vm->strings, string, NULL_VALUE);
}

void vm_register_value(VM* vm, LinkedValue* value) {
    value->next = vm->values;
    vm->values = value;
}