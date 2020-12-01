#include "vm.h"

#include "../memory/gc.h"
#include "../memory/mem.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../value/dense.h"
#include "../asm/disassembler.h"

#define VM_RUNTIME_ERROR(vm, fmt, ...) \
    fprintf(stderr, "[error] at index %u: " fmt "\n", FRAME_FUNCTION(vm->frames[vm->frameCount - 1])->chunk.indices[vm->frames[vm->frameCount - 1].ip - FRAME_FUNCTION(vm->frames[vm->frameCount - 1])->chunk.bytecode], ##__VA_ARGS__ )

static bool call_value(VM* vm, uint8_t reg, uint8_t argc);
static bool call_function(VM* vm, uint8_t reg, uint8_t argc);
static bool call_closure(VM* vm, uint8_t reg, uint8_t argc);
static bool call_native(VM* vm, uint8_t reg, uint8_t argc);

static DenseUpvalue* upvalue_capture(VM* vm, Value* local);
static void          upvalue_close_from(VM* vm, Value* slot);

void vm_init(VM* vm) {
    vm_stack_reset(vm);

    map_init(&vm->strings);
    map_init(&vm->globals);

    vm->values = NULL;
    vm->options.replMode = false;
    vm->heapSize = 0;
    vm->heapThreshold = 64 * 1024;
}

void vm_delete(VM* vm) {
    map_delete(&vm->strings);
    map_delete(&vm->globals);

    DenseValue* dense = vm->values;

    while(dense != NULL) {
        DenseValue* next = dense->link;
        vm->heapSize -= dense_size(dense);
        dense_delete(dense);
        dense = next;
    }
}

void vm_clean(VM* vm) {
    while(vm->frameCount > 1) {
        CallFrame* frame = &vm->frames[vm->frameCount - 1];

        switch(frame->type) {
            case FRAME_FUNCTION:
                dense_delete((DenseValue*) frame->callee.function);
                break;
            case FRAME_CLOSURE:
                dense_delete((DenseValue*) frame->callee.closure);
                break;
        }
    }

    vm_stack_reset(vm);

    gc_check(vm);
}

VMStatus vm_execute(VM* vm) {
    return vm_run(vm);
}

VMStatus vm_run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    #define TYPE_MASK 0xC0
    #define INSTRUCTION_MASK 0x3F

    #define NEXT_BYTE() (*frame->ip++)
    #define NEXT_CONSTANT() (FRAME_FUNCTION(*frame)->chunk.constants.values[NEXT_BYTE()])

    #define DEST     (*frame->ip)
    #define LEFT     (frame->ip[1])
    #define RIGHT    (frame->ip[2])
    #define COMBINED ((((uint16_t) frame->ip[1]) << 8) | (frame->ip[2]))

    #define DEST_CONST     (FRAME_FUNCTION(*frame)->chunk.constants.values[DEST])
    #define LEFT_CONST     (FRAME_FUNCTION(*frame)->chunk.constants.values[LEFT])
    #define RIGHT_CONST    (FRAME_FUNCTION(*frame)->chunk.constants.values[RIGHT])
    #define COMBINED_CONST (FRAME_FUNCTION(*frame)->chunk.constants.values[COMBINED])

    #define DEST_REG  (frame->regs[DEST])
    #define LEFT_REG  (frame->regs[LEFT])
    #define RIGHT_REG (frame->regs[RIGHT])

    #define LEFT_TYPE_MASK 0x80
    #define RIGHT_TYPE_MASK 0x40

    #define LEFT_BY_TYPE  (types & LEFT_TYPE_MASK ? LEFT_CONST : LEFT_REG)
    #define RIGHT_BY_TYPE (types & RIGHT_TYPE_MASK ? RIGHT_CONST : RIGHT_REG)

    #define SKIP(count) (frame->ip += count)
    #define BSKIP(count) (frame->ip -= count)

    while(1) {
        #ifdef DEBUG_TRACE_EXECUTION
            debug_disassemble_instruction(&FRAME_FUNCTION(*frame)->chunk, (uint32_t) (frame->ip - FRAME_FUNCTION(*frame)->chunk.bytecode));

            PRINT("          ");
            for(Value* entry = vm->stack; entry < vm->stackTop + 7; ++entry) {
                PRINT("[ ");
                value_print(*entry);
                PRINT(" ]");
            }
            PRINT("\n");
        #endif

        uint8_t instruction = NEXT_BYTE();
        uint8_t types = instruction & TYPE_MASK;
        instruction &= INSTRUCTION_MASK;

        switch(instruction) {
            case OP_CNST: {
                DEST_REG = LEFT_CONST;

                SKIP(3);
                break;
            }
            case OP_CNSTW: {
                DEST_REG = COMBINED_CONST;

                SKIP(3);
                break;
            }
            case OP_MOV: {
                DEST_REG = LEFT_REG;

                SKIP(3);
                break;
            }
            case OP_CLONE: {
                DEST_REG = value_clone(LEFT_REG);

                if(DEST_REG.type == VAL_DENSE) {
                    switch(AS_DENSE(DEST_REG)->type) {
                        case DVAL_ARRAY:
                        case DVAL_OBJECT:
                        case DVAL_UPVALUE:
                            vm_register_dense(vm, AS_DENSE(DEST_REG));
                        default:
                            break;
                    }
                }

                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_DGLOB: {
                map_set(&vm->globals, AS_STRING(DEST_CONST), LEFT_BY_TYPE);
                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_GGLOB: {
                Value value;

                map_get(&vm->globals, AS_STRING(LEFT_CONST), &value);

                if(!map_get(&vm->globals, AS_STRING(LEFT_CONST), &value)) {
                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(LEFT_CONST));
                    return VM_ERROR;
                }

                DEST_REG = value;

                SKIP(3);
                break;
            }
            case OP_SGLOB: {
                if(map_set(&vm->globals, AS_STRING(DEST_CONST), LEFT_BY_TYPE)) {
                    map_erase(&vm->globals, AS_STRING(DEST_CONST));

                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", AS_CSTRING(DEST_CONST));
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_UPVAL: {
                VM_RUNTIME_ERROR(vm, "Illegal instruction 'UPVAL'");
                return VM_ERROR;
            }
            case OP_GUPVAL: {
                if(frame->type != FRAME_CLOSURE) {
                    VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                    return VM_ERROR;
                }

                DEST_REG = *frame->callee.closure->upvalues[LEFT]->ref;

                SKIP(3);
                break;
            }
            case OP_SUPVAL: {
                if(frame->type != FRAME_CLOSURE) {
                    VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                    return VM_ERROR;
                }

                *frame->callee.closure->upvalues[DEST]->ref = LEFT_REG;

                SKIP(3);
                break;
            }
            case OP_CUPVAL: {
                upvalue_close_from(vm, &DEST_REG);

                SKIP(3);
                break;
            }
            case OP_CLSR: {
                DenseFunction* function = (DenseFunction*) AS_DENSE(LEFT_REG);
                DenseClosure* closure = dense_closure_create(function, RIGHT);

                vm_register_dense(vm, (DenseValue*) closure);

                DEST_REG = DENSE_VALUE(closure);

                for(uint8_t i = 0; i < RIGHT; ++i) {
                    SKIP(4);

                    uint8_t index = DEST;
                    bool local = LEFT;

                    if(local)
                        closure->upvalues[i] = upvalue_capture(vm, frame->regs + index);
                    else {
                        if(frame->type != FRAME_CLOSURE) {
                            VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                            return VM_ERROR;
                        }

                        closure->upvalues[i] = frame->callee.closure->upvalues[i];
                    }
                }

                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_LEN: {
                if(value_is_dense_of_type(LEFT_REG, DVAL_ARRAY)) {
                    DEST_REG = INT_VALUE(AS_ARRAY(LEFT_REG)->data.size);
                } else if(value_is_dense_of_type(LEFT_REG, DVAL_STRING)) {
                    DEST_REG = INT_VALUE(AS_STRING(LEFT_REG)->length);
                } else {
                    VM_RUNTIME_ERROR(vm, "Expected string or array");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_ARR: {
                DEST_REG = DENSE_VALUE(dense_array_create());
                vm_register_dense(vm, AS_DENSE(DEST_REG));
                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_PARR: {
                if(!value_is_dense_of_type(DEST_REG, DVAL_ARRAY)) {
                    VM_RUNTIME_ERROR(vm, "Destination must be an array");
                    return VM_ERROR;
                }

                DenseArray* array = AS_ARRAY(DEST_REG);

                if(array->data.size == UINT32_MAX) {
                    VM_RUNTIME_ERROR(vm, "Array size limit exceeded (4294967295)");
                    return VM_ERROR;
                }

                value_array_write(&array->data, LEFT_BY_TYPE);
                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_OBJ: {
                DEST_REG = DENSE_VALUE(dense_object_create());
                vm_register_dense(vm, AS_DENSE(DEST_REG));
                gc_check(vm);

                SKIP(3);
                break;
            }
            case OP_GET: {
                if(value_is_dense_of_type(LEFT_REG, DVAL_ARRAY)) {
                    if(!IS_INT(RIGHT_BY_TYPE)) {
                        VM_RUNTIME_ERROR(vm, "Index must be int");
                        return VM_ERROR;
                    }

                    DenseArray* array = AS_ARRAY(LEFT_REG);
                    uint32_t index = AS_INT(RIGHT_BY_TYPE);

                    if(index >= array->data.size) {
                        VM_RUNTIME_ERROR(vm, "Index out of bounds");
                        return VM_ERROR;
                    }

                    DEST_REG = dense_array_get(array, index);
                } else if(value_is_dense_of_type(LEFT_REG, DVAL_STRING)) {
                    if(!IS_INT(RIGHT_BY_TYPE)) {
                        VM_RUNTIME_ERROR(vm, "Index must be int");
                        return VM_ERROR;
                    }

                    DenseString* str = AS_STRING(LEFT_REG);
                    uint32_t index = AS_INT(RIGHT_BY_TYPE);

                    if(index >= str->length) {
                        VM_RUNTIME_ERROR(vm, "Index out of bounds");
                        return VM_ERROR;
                    }

                    DenseString* result = dense_string_from(str->chars + index, 1);
                    DenseString* interned = map_find(&vm->strings, result->chars + index, 1, map_hash(str->chars + index, 1));

                    if(interned != NULL) {
                        MEM_FREE(result);
                        result = interned;
                        DEST_REG = DENSE_VALUE(result);
                    } else {
                        vm_register_string(vm, result);
                        vm_register_dense(vm, (DenseValue*) result);
                        DEST_REG = DENSE_VALUE(result);
                        gc_check(vm);
                    }
                } else if(value_is_dense_of_type(LEFT_REG, DVAL_OBJECT)) {
                    if(!value_is_dense_of_type(RIGHT_BY_TYPE, DVAL_STRING)) {
                        VM_RUNTIME_ERROR(vm, "Object key must be string");
                        return VM_ERROR;
                    }

                    DenseObject* object = AS_OBJECT(LEFT_REG);
                    DenseString* key = AS_STRING(RIGHT_BY_TYPE);

                    Value value;

                    if(!dense_object_get(object, key, &value)) {
                        VM_RUNTIME_ERROR(vm, "Object property does not exist");
                        return VM_ERROR;
                    }

                    DEST_REG = value;
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be an array or object");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_SET: {
                if(value_is_dense_of_type(DEST_REG, DVAL_ARRAY)) {
                    if(!IS_INT(LEFT_BY_TYPE)) {
                        VM_RUNTIME_ERROR(vm, "Index must be int");
                        return VM_ERROR;
                    }

                    DenseArray* array = AS_ARRAY(DEST_REG);
                    uint32_t index = AS_INT(LEFT_BY_TYPE);

                    if(index > array->data.size) {
                        VM_RUNTIME_ERROR(vm, "Index out of bounds");
                        return VM_ERROR;
                    } else if(index == array->data.size) {
                        if(array->data.size == UINT32_MAX) {
                            VM_RUNTIME_ERROR(vm, "Array size limit exceeded (4294967295)");
                            return VM_ERROR;
                        }

                        value_array_write(&array->data, RIGHT_BY_TYPE);
                        gc_check(vm);
                    } else dense_array_set(array, index, RIGHT_BY_TYPE);
                } else if(value_is_dense_of_type(DEST_REG, DVAL_OBJECT)) {
                    if(!value_is_dense_of_type(LEFT_BY_TYPE, DVAL_STRING)) {
                        VM_RUNTIME_ERROR(vm, "Object key must be string");
                        return VM_ERROR;
                    }

                    DenseObject* object = AS_OBJECT(DEST_REG);
                    DenseString* key = AS_STRING(LEFT_BY_TYPE);

                    dense_object_set(object, key, RIGHT_BY_TYPE);

                    gc_check(vm);
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be an array or object");
                    return VM_ERROR;
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
                DEST_REG = BOOL_VALUE(value_is_falsy(LEFT_BY_TYPE));

                SKIP(3);
                break;
            }
            case OP_BNOT: {
                Value left = LEFT_BY_TYPE;

                if(IS_BYTE(left))
                    DEST_REG = BYTE_VALUE(~AS_BYTE(left));
                else if(IS_INT(left))
                    DEST_REG = INT_VALUE(~AS_INT(left));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte or int");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_NEG: {
                Value left = LEFT_BY_TYPE;

                if(IS_BYTE(left))
                    DEST_REG = INT_VALUE(-((int64_t) AS_BYTE(left)));
                else if(IS_INT(left))
                    DEST_REG = INT_VALUE(-AS_INT(left));
                else if(IS_FLOAT(left))
                    DEST_REG = FLOAT_VALUE(-AS_FLOAT(left));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_INC: {
                Value dest = DEST_REG;

                if(IS_BYTE(dest))
                    ++AS_BYTE(DEST_REG);
                else if(IS_INT(dest))
                    ++AS_INT(DEST_REG);
                else if(IS_FLOAT(dest))
                    ++AS_FLOAT(DEST_REG);
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_DEC: {
                Value dest = DEST_REG;

                if(IS_BYTE(dest))
                    --AS_BYTE(DEST_REG);
                else if(IS_INT(dest))
                    --AS_INT(DEST_REG);
                else if(IS_FLOAT(dest))
                    --AS_FLOAT(DEST_REG);
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return VM_ERROR;
                }

                SKIP(3);
                break;
            }
            case OP_ADD: {
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) + AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) + AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(left) + AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) + AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) + AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(left) + AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) + AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) + AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) + AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(value_is_dense_of_type(left, DVAL_STRING)) {
                    if(value_is_dense_of_type(right, DVAL_STRING)) {
                        DenseString* result = dense_string_concat(AS_STRING(left), AS_STRING(right));
                        DenseString* interned = map_find(&vm->strings, result->chars, result->length, result->hash);

                        if(interned != NULL) {
                            MEM_FREE(result);
                            result = interned;
                            DEST_REG = DENSE_VALUE(result);
                        } else {
                            vm_register_string(vm, result);
                            vm_register_dense(vm, (DenseValue*) result);
                            DEST_REG = DENSE_VALUE(result);
                            gc_check(vm);
                        }
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) - AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) - AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(left) - AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) - AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) - AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(left) - AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) - AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) - AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) - AS_FLOAT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) * AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) * AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(left) * AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) * AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) * AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(left) * AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) * AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) * AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) * AS_FLOAT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) / AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) / AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_BYTE(left) / AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) / AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) / AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_INT(left) / AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) / AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) / AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = FLOAT_VALUE(AS_FLOAT(left) / AS_FLOAT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) % AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) % AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_INT(left) % AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) % AS_INT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) << AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        if(AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = BYTE_VALUE(AS_BYTE(left) << AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(AS_INT(left) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return VM_ERROR;
                    }

                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) << AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        if(AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = INT_VALUE(AS_INT(left) << AS_INT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) >> AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        if(AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = BYTE_VALUE(AS_BYTE(left) >> AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(AS_INT(left) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return VM_ERROR;
                    }

                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) >> AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        if(AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return VM_ERROR;
                        }

                        DEST_REG = INT_VALUE(AS_INT(left) >> AS_INT(right));
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
            case OP_LT: {
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) < AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) < AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) < AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) < AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) < AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) < AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) < AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) < AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) < AS_FLOAT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) <= AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) <= AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_BYTE(left) <= AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) <= AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) <= AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_INT(left) <= AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return VM_ERROR;
                    }
                } else if(IS_FLOAT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) <= AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) <= AS_INT(right));
                    } else if(IS_FLOAT(right)) {
                        DEST_REG = BOOL_VALUE(AS_FLOAT(left) <= AS_FLOAT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                DEST_REG = BOOL_VALUE(value_equals(left, right));

                SKIP(3);
                break;
            }
            case OP_NEQ: {
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                DEST_REG = BOOL_VALUE(!value_equals(left, right));

                SKIP(3);
                break;
            }
            case OP_BAND: {
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) & AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) & AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) & AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) & AS_INT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) ^ AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) ^ AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) ^ AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) ^ AS_INT(right));
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
                Value left = LEFT_BY_TYPE;
                Value right = RIGHT_BY_TYPE;

                if(IS_BYTE(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = BYTE_VALUE(AS_BYTE(left) | AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_BYTE(left) | AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return VM_ERROR;
                    }
                } else if(IS_INT(left)) {
                    if(IS_BYTE(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) | AS_BYTE(right));
                    } else if(IS_INT(right)) {
                        DEST_REG = INT_VALUE(AS_INT(left) | AS_INT(right));
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
                uint16_t amount = *((uint16_t*) &frame->ip); // This takes DEST and LEFT as 16 bits.

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
                uint16_t amount = *((uint16_t*) &frame->ip); // This takes DEST and LEFT as 16 bits.

                BSKIP(amount * 4);
                BSKIP(1);
                break;
            }
            case OP_CALL: {
                if(!call_value(vm, DEST, LEFT))
                    return VM_ERROR;

                // Native function.
                if(frame == &vm->frames[vm->frameCount - 1])
                    SKIP(3);
                else frame = &vm->frames[vm->frameCount - 1];

                break;
            }
            case OP_RET: {
                upvalue_close_from(vm, frame->regs);

                --vm->frameCount;

                if(vm->frameCount == 0)
                    return VM_OK;

                if(DEST > 249)
                    *frame->base = NULL_VALUE;
                else *frame->base = DEST_REG;

                vm->stackTop = frame->base + 1;
                frame = &vm->frames[vm->frameCount - 1];

                SKIP(3); // Skip the CALL args.
                break;
            }
            case OP_ACC: {
                if(DEST > 249) {
                    VM_RUNTIME_ERROR(vm, "Expected register");
                    return VM_ERROR;
                }

                vm->acc = DEST_REG;

                SKIP(3);
                break;
            }
            case OP_DIS: {
                DenseFunction* func;

                if(DEST > 249)
                    func = FRAME_FUNCTION(*frame);
                else {
                    if(value_is_dense_of_type(DEST_REG, DVAL_FUNCTION))
                        func = AS_FUNCTION(DEST_REG);
                    else if(value_is_dense_of_type(DEST_REG, DVAL_CLOSURE))
                        func = AS_CLOSURE(DEST_REG)->function;
                    else {
                        VM_RUNTIME_ERROR(vm, "Argument must be a non-native function");
                        return VM_ERROR;
                    }
                }

                PRINT("\n");
                dense_print((DenseValue*) func);
                disassembler_process_chunk(&func->chunk);
                PRINT("\n\n");

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

    #undef LEFT_CONST

    #undef RIGHT
    #undef LEFT
    #undef DEST

    #undef NEXT_CONSTANT
    #undef NEXT_BYTE
}

static bool call_value(VM* vm, uint8_t reg, uint8_t argc) {
    Value value = vm->frames[vm->frameCount - 1].regs[reg];

    if(IS_DENSE(value)) {
        switch(AS_DENSE(value)->type) {
            case DVAL_FUNCTION:
                return call_function(vm, reg, argc);
            case DVAL_CLOSURE:
                return call_closure(vm, reg, argc);
            case DVAL_NATIVE:
                return call_native(vm, reg, argc);
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
    frame->type = FRAME_FUNCTION;
    frame->callee.function = function;
    frame->ip = function->chunk.bytecode;

    frame->base = &vm->frames[vm->frameCount - 2].regs[reg];
    frame->regs = frame->base + 1;

    vm->stackTop += 251;

    return true;
}

static bool call_closure(VM* vm, uint8_t reg, uint8_t argc) {
    DenseClosure* closure = AS_CLOSURE(vm->frames[vm->frameCount - 1].regs[reg]);
    DenseFunction* function = closure->function;

    if(argc != function->arity) {
        VM_RUNTIME_ERROR(vm, "Expected %x args, got %x", function->arity, argc);
        return false;
    }

    if(vm->frameCount == CALLFRAME_STACK_SIZE) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->type = FRAME_CLOSURE;
    frame->callee.closure = closure;
    frame->ip = function->chunk.bytecode;

    frame->base = &vm->frames[vm->frameCount - 2].regs[reg];
    frame->regs = frame->base + 1;

    vm->stackTop += 251;

    return true;
}

static bool call_native(VM* vm, uint8_t reg, uint8_t argc) {
    #define FRAME vm->frames[vm->frameCount - 1]

    DenseNative* native = AS_NATIVE(FRAME.regs[reg]);

    vm->frames[vm->frameCount - 1].regs[reg] = native->function(vm, argc, &FRAME.regs[reg + 1]);
    return true;

    #undef FRAME
}

static DenseUpvalue* upvalue_capture(VM* vm, Value* local) {
    DenseUpvalue* prev = NULL;
    DenseUpvalue* upvalue = vm->upvalues;

    while(upvalue != NULL && upvalue->ref > local) {
        prev = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->ref == local)
        return upvalue;

    DenseUpvalue* created = dense_upvalue_create(local);
    created->next = upvalue;

    if(prev == NULL)
        vm->upvalues = created;
    else prev->next = created;

    vm_register_dense(vm, (DenseValue*) created);

    return created;
}

static void upvalue_close_from(VM* vm, Value* slot) {
    while(vm->upvalues != NULL && vm->upvalues->ref >= slot) {
        DenseUpvalue* head = vm->upvalues;
        head->closed = *head->ref;
        head->ref = &head->closed;
        vm->upvalues = head->next;
    }
}

#undef VM_RUNTIME_ERROR