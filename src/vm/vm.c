#include "vm.h"

#include "../mem/gc.h"
#include "../mem/mem.h"
#include "../cluster/bytecode.h"
#include "../io/log.h"
#include "../value/dense.h"
#include "../asm/disassembler.h"
#include "../compiler/compiler.h"
#include "../def/macro.h"

#define VM_RUNTIME_ERROR(vm, fmt, ...) \
    fprintf(stderr, "[error] at index %u: " fmt "\n", VM_FRAME_FUNCTION(vm->frames[vm->frameCount - 1])->cluster.indices[vm->frames[vm->frameCount - 1].ip - VM_FRAME_FUNCTION(vm->frames[vm->frameCount - 1])->cluster.bytecode], ##__VA_ARGS__ )

#ifdef DEBUG
    #define VM_DEBUG_CHECK_STACK                                                          \
        do {                                                                              \
            if(vm->stack + RISA_VM_STACK_SIZE < vm->stackTop || vm->stackTop < vm->stack) \
                RISA_PANIC("VM stack top out of bounds");                                 \
        } while(false)
#else
    #define VM_DEBUG_CHECK_STACK
#endif

static bool risa_vm_call_register (RisaVM*, uint8_t, uint8_t);
static bool risa_vm_call_value    (RisaVM*, RisaValue*, RisaValue, uint8_t, bool);
static bool risa_vm_call_function (RisaVM*, RisaValue*, RisaValue, uint8_t, bool);
static bool risa_vm_call_closure  (RisaVM*, RisaValue*, RisaValue, uint8_t, bool);
static bool risa_vm_call_native   (RisaVM*, RisaValue*, RisaValue, uint8_t, bool);
static RisaValue risa_vm_invoke_directly(RisaVM*, RisaValue*, RisaValue, uint8_t);

static RisaDenseUpvalue* risa_vm_upvalue_capture    (RisaVM* vm, RisaValue* local);
static void              risa_vm_upvalue_close_from (RisaVM* vm, RisaValue* slot);

RisaVM* risa_vm_create() {
    RisaVM* vm = RISA_MEM_ALLOC(sizeof(RisaVM));

    risa_vm_init(vm);

    return vm;
}

void risa_vm_init(RisaVM* vm) {
    risa_io_init(&vm->io);

    risa_vm_stack_reset(vm);

    risa_map_init(&vm->strings);
    risa_map_init(&vm->globals);

    vm->frameCount = 0;
    vm->values = NULL;
    vm->acc = RISA_NULL_VALUE;
    vm->options.replMode = false; // TODO: Split compiler and vm options into separate structs.
    vm->heapSize = 0;
    vm->heapThreshold = RISA_VM_HEAP_INITIAL_THRESHOLD;
}

void risa_vm_delete(RisaVM* vm) {
    risa_map_delete(&vm->strings);
    risa_map_delete(&vm->globals);

    RisaDenseValue* dense = vm->values;

    while(dense != NULL) {
        RisaDenseValue* next = dense->link;
        vm->heapSize -= risa_dense_size(dense);
        risa_dense_delete(dense);
        dense = next;
    }
}

void risa_vm_free(RisaVM* vm) {
    risa_vm_delete(vm);

    RISA_MEM_FREE(vm);
}

void risa_vm_clean(RisaVM* vm) {
    while(vm->frameCount > 1) {
        RisaCallFrame* frame = &vm->frames[vm->frameCount - 1];

        switch(frame->type) {
            case RISA_FRAME_FUNCTION:
                risa_dense_delete((RisaDenseValue *) frame->callee.function);
                break;
            case RISA_FRAME_CLOSURE:
                risa_dense_delete((RisaDenseValue *) frame->callee.closure);
                break;
        }
    }

    risa_vm_stack_reset(vm);
    risa_gc_run(vm);
}

void risa_vm_load_compiler_data(RisaVM* vm, void* compiler) {
    risa_vm_load_strings(vm, &((RisaCompiler*) compiler)->strings);
}

void risa_vm_load_function(RisaVM* vm, RisaDenseFunction* function) {
    vm->frames[0] = risa_vm_frame_from_function(vm, NULL, function, true);

    risa_vm_register_dense(vm, (RisaDenseValue*) function);

    if(vm->frameCount > 1)
        risa_vm_clean(vm);
    else risa_vm_stack_reset(vm);

    vm->acc = RISA_NULL_VALUE;
    vm->frameCount = 1;
    vm->stackTop += 250;
}

void risa_vm_load_strings(RisaVM* vm, RisaMap* strings) {
    vm->strings = *strings;
}

RisaMap* risa_vm_get_strings(RisaVM* vm) {
    return &vm->strings;
}

RisaIO* risa_vm_get_io(RisaVM* vm) {
    return &vm->io;
}

RisaValue risa_vm_get_acc(RisaVM* vm) {
    return vm->acc;
}

void risa_vm_set_repl_mode(RisaVM* vm, bool mode) {
    vm->options.replMode = mode;
}

RisaVMStatus risa_vm_execute(RisaVM* vm) {
    return risa_vm_run(vm, 0);
}

RisaVMStatus risa_vm_run(RisaVM* vm, uint32_t maxInstr) {
    RisaCallFrame* frame = &vm->frames[vm->frameCount - 1];
    bool forever = maxInstr == 0;

    #define NEXT_BYTE()     (*frame->ip++)
    #define NEXT_CONSTANT() (VM_FRAME_FUNCTION(*frame)->cluster.constants.values[NEXT_BYTE()])

    #define DEST            (*frame->ip)
    #define LEFT            (frame->ip[1])
    #define RIGHT           (frame->ip[2])
    #define COMBINED        ((((uint16_t) frame->ip[1]) << 8) | (frame->ip[2]))

    #define DEST_CONST      (VM_FRAME_FUNCTION(*frame)->cluster.constants.values[DEST])
    #define LEFT_CONST      (VM_FRAME_FUNCTION(*frame)->cluster.constants.values[LEFT])
    #define RIGHT_CONST     (VM_FRAME_FUNCTION(*frame)->cluster.constants.values[RIGHT])
    #define COMBINED_CONST  (VM_FRAME_FUNCTION(*frame)->cluster.constants.values[COMBINED])

    #define DEST_REG        (frame->regs[DEST])
    #define LEFT_REG        (frame->regs[LEFT])
    #define RIGHT_REG       (frame->regs[RIGHT])

    #define DEST_BY_TYPE    (types & RISA_TODLR_TYPE_LEFT_MASK ? DEST_CONST : DEST_REG)
    #define LEFT_BY_TYPE    (types & RISA_TODLR_TYPE_LEFT_MASK ? LEFT_CONST : LEFT_REG)
    #define RIGHT_BY_TYPE   (types & RISA_TODLR_TYPE_RIGHT_MASK ? RIGHT_CONST : RIGHT_REG)

    #define SKIP(count)     (frame->ip += count)
    #define BSKIP(count)    (frame->ip -= count)

    while(1) {
        VM_DEBUG_CHECK_STACK;

        uint8_t instruction = NEXT_BYTE();
        uint8_t types = instruction & RISA_TODLR_TYPE_MASK;
        instruction &= RISA_TODLR_INSTRUCTION_MASK;

        switch(instruction) {
            case RISA_OP_CNST: {
                DEST_REG = LEFT_CONST;

                SKIP(3);
                break;
            }
            case RISA_OP_CNSTW: {
                DEST_REG = COMBINED_CONST;

                SKIP(3);
                break;
            }
            case RISA_OP_MOV: {
                DEST_REG = LEFT_REG;

                SKIP(3);
                break;
            }
            case RISA_OP_CLONE: {
                DEST_REG = risa_value_clone_register(vm, LEFT_REG);

                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_DGLOB: {
                risa_map_set(&vm->globals, RISA_AS_STRING(DEST_CONST), LEFT_BY_TYPE);
                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_GGLOB: {
                RisaValue value;

                if(!risa_map_get(&vm->globals, RISA_AS_STRING(LEFT_CONST), &value)) {
                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", RISA_AS_CSTRING(LEFT_CONST));
                    return RISA_VM_STATUS_ERROR;
                }

                DEST_REG = value;

                SKIP(3);
                break;
            }
            case RISA_OP_SGLOB: {
                if(risa_map_set(&vm->globals, RISA_AS_STRING(DEST_CONST), LEFT_BY_TYPE)) {
                    risa_map_erase(&vm->globals, RISA_AS_STRING(DEST_CONST));

                    VM_RUNTIME_ERROR(vm, "Undefined variable '%s'", RISA_AS_CSTRING(DEST_CONST));
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_UPVAL: {
                VM_RUNTIME_ERROR(vm, "Illegal instruction 'UPVAL'; must be after 'CLSR'");
                return RISA_VM_STATUS_ERROR;
            }
            case RISA_OP_GUPVAL: {
                if(frame->type != RISA_FRAME_CLOSURE) {
                    VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                    return RISA_VM_STATUS_ERROR;
                }

                DEST_REG = *(frame->callee.closure->upvalues[LEFT]->ref);

                SKIP(3);
                break;
            }
            case RISA_OP_SUPVAL: {
                if(frame->type != RISA_FRAME_CLOSURE) {
                    VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                    return RISA_VM_STATUS_ERROR;
                }

                *frame->callee.closure->upvalues[DEST]->ref = LEFT_REG;

                SKIP(3);
                break;
            }
            case RISA_OP_CUPVAL: {
                risa_vm_upvalue_close_from(vm, &DEST_REG);

                SKIP(3);
                break;
            }
            case RISA_OP_CLSR: {
                // TODO: Check if right reg is a dense function, in case someone injects bytecode with a CLSR instruction.
                RisaDenseFunction* function = (RisaDenseFunction*) RISA_AS_DENSE(LEFT_REG);
                RisaDenseClosure* closure = risa_dense_closure_create(function, RIGHT);

                risa_vm_register_dense(vm, (RisaDenseValue *) closure);

                DEST_REG = RISA_DENSE_VALUE(closure);

                uint8_t upvalCount = RIGHT;

                for(uint8_t i = 0; i < upvalCount; ++i) {
                    SKIP(4);

                    uint8_t index = DEST;
                    bool local = LEFT;

                    if(local)
                        closure->upvalues[i] = risa_vm_upvalue_capture(vm, frame->regs + index);
                    else {
                        if(frame->type != RISA_FRAME_CLOSURE) {
                            VM_RUNTIME_ERROR(vm, "Frame not of type 'closure'");
                            return RISA_VM_STATUS_ERROR;
                        }

                        closure->upvalues[i] = frame->callee.closure->upvalues[index];
                    }
                }

                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_LEN: {
                switch(LEFT_REG.type) {
                    case RISA_VAL_DENSE:
                        switch(RISA_AS_DENSE(LEFT_REG)->type) {
                            case RISA_DVAL_ARRAY:
                                DEST_REG = RISA_INT_VALUE(RISA_AS_ARRAY(LEFT_REG)->data.size);
                                goto _op_len_success;
                            case RISA_DVAL_STRING:
                                DEST_REG = RISA_INT_VALUE(RISA_AS_STRING(LEFT_REG)->length);
                                goto _op_len_success;
                            default:
                                break;
                        }
                        // Fallthrough
                    default:
                        VM_RUNTIME_ERROR(vm, "Expected string or array");
                        return RISA_VM_STATUS_ERROR;
                }

            _op_len_success:

                SKIP(3);
                break;
            }
            case RISA_OP_ARR: {
                DEST_REG = RISA_DENSE_VALUE(risa_dense_array_create());
                risa_vm_register_dense(vm, RISA_AS_DENSE(DEST_REG));
                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_PARR: {
                if(!risa_value_is_dense_of_type(DEST_REG, RISA_DVAL_ARRAY)) {
                    VM_RUNTIME_ERROR(vm, "Destination must be an array");
                    return RISA_VM_STATUS_ERROR;
                }

                RisaDenseArray* array = RISA_AS_ARRAY(DEST_REG);

                if(array->data.size == UINT32_MAX) {
                    VM_RUNTIME_ERROR(vm, "Array size limit exceeded (4294967295)");
                    return RISA_VM_STATUS_ERROR;
                }

                risa_value_array_write(&array->data, LEFT_BY_TYPE);
                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_OBJ: {
                DEST_REG = RISA_DENSE_VALUE(risa_dense_object_create());
                risa_vm_register_dense(vm, RISA_AS_DENSE(DEST_REG));
                risa_gc_check(vm);

                SKIP(3);
                break;
            }
            case RISA_OP_GET: {
                switch(LEFT_REG.type) {
                    case RISA_VAL_DENSE:
                        switch(RISA_AS_DENSE(LEFT_REG)->type) {
                            case RISA_DVAL_ARRAY: {
                                if(!RISA_IS_INT(RIGHT_BY_TYPE)) {
                                    VM_RUNTIME_ERROR(vm, "Index must be int");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseArray* array = RISA_AS_ARRAY(LEFT_REG);
                                int64_t index = RISA_AS_INT(RIGHT_BY_TYPE);

                                if(index < 0 || index >= array->data.size) {
                                    VM_RUNTIME_ERROR(vm, "Index out of bounds");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                DEST_REG = risa_dense_array_get(array, (uint32_t) index);

                                goto _op_get_success;
                            }
                            case RISA_DVAL_STRING: {
                                if(!RISA_IS_INT(RIGHT_BY_TYPE)) {
                                    VM_RUNTIME_ERROR(vm, "Index must be int");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseString* str = RISA_AS_STRING(LEFT_REG);
                                int64_t index = RISA_AS_INT(RIGHT_BY_TYPE);

                                if(index < 0 || index >= str->length) {
                                    VM_RUNTIME_ERROR(vm, "Index out of bounds");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                DEST_REG = RISA_DENSE_VALUE(risa_vm_string_create(vm, str->chars + index, 1));

                                risa_gc_check(vm);

                                goto _op_get_success;
                            }
                            case RISA_DVAL_OBJECT: {
                                if(!risa_value_is_dense_of_type(RIGHT_BY_TYPE, RISA_DVAL_STRING)) {
                                    VM_RUNTIME_ERROR(vm, "Object key must be string");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseObject* object = RISA_AS_OBJECT(LEFT_REG);
                                RisaDenseString* key = RISA_AS_STRING(RIGHT_BY_TYPE);

                                RisaValue value;

                                if(!risa_dense_object_get(object, key, &value)) {
                                    VM_RUNTIME_ERROR(vm, "Object property does not exist");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                DEST_REG = value;

                                goto _op_get_success;
                            }
                            default:
                                break;
                        }
                        // Fallthrough
                    default:
                        VM_RUNTIME_ERROR(vm, "Left operand must be an array or object");
                        return RISA_VM_STATUS_ERROR;
                }

            _op_get_success:

                SKIP(3);
                break;
            }
            case RISA_OP_SET: {
                switch(DEST_REG.type) {
                    case RISA_VAL_DENSE:
                        switch(RISA_AS_DENSE(DEST_REG)->type) {
                            case RISA_DVAL_ARRAY: {
                                if(!RISA_IS_INT(LEFT_BY_TYPE)) {
                                    VM_RUNTIME_ERROR(vm, "Index must be int");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseArray* array = RISA_AS_ARRAY(DEST_REG);
                                int64_t index = RISA_AS_INT(LEFT_BY_TYPE);

                                if(index < 0 || index > array->data.size) {
                                    VM_RUNTIME_ERROR(vm, "Index out of bounds");
                                    return RISA_VM_STATUS_ERROR;
                                } else if(index == array->data.size) {
                                    if(array->data.size == UINT32_MAX) {
                                        VM_RUNTIME_ERROR(vm, "Array size limit exceeded (4294967295)");
                                        return RISA_VM_STATUS_ERROR;
                                    }

                                    risa_value_array_write(&array->data, RIGHT_BY_TYPE);
                                    risa_gc_check(vm);
                                } else risa_dense_array_set(array, (uint32_t) index, RIGHT_BY_TYPE);

                                goto _op_set_success;
                            }
                            /*case RISA_DVAL_STRING: {
                                if(!RISA_IS_INT(LEFT_BY_TYPE)) {
                                    VM_RUNTIME_ERROR(vm, "Index must be int");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                if(!risa_value_is_dense_of_type(RIGHT_BY_TYPE, RISA_DVAL_STRING)) {
                                    VM_RUNTIME_ERROR(vm, "Right operand must be a string of length 1");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseString* str = RISA_AS_STRING(DEST_REG);
                                int64_t index = RISA_AS_INT(LEFT_BY_TYPE);
                                RisaDenseString* chr = RISA_AS_STRING(RIGHT_BY_TYPE);

                                if(chr->length != 1) {
                                    VM_RUNTIME_ERROR(vm, "Right operand must be a string of length 1");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                if(index < 0 || index > str->length) {
                                    VM_RUNTIME_ERROR(vm, "Index out of bounds");
                                    return RISA_VM_STATUS_ERROR;
                                } else if(index == str->length) {
                                    // The string will end with two nulls: one from the original str (because length + 1
                                    // oversteps into the null character), and one added by the function. The first null
                                    // will be replaced with the character. This practically appends a character to the string.
                                    RisaDenseString* newStr = dense_string_prepare(str->chars, str->length + 1);

                                    newStr->chars[index] = chr->chars[0];
                                    dense_string_hash_inplace(newStr);

                                    newStr = vm_string_internalize(vm, newStr);

                                    DEST_REG = RISA_DENSE_VALUE((RisaDenseValue*) newStr);

                                    gc_check(vm);
                                } else {
                                    if(str->chars[index] != chr->chars[0]) {
                                        RisaDenseString* newStr = risa_dense_string_prepare(str->chars, str->length);

                                        newStr->chars[index] = chr->chars[0];
                                        risa_dense_string_hash_inplace(newStr);

                                        newStr = risa_vm_string_internalize(vm, newStr);

                                        DEST_REG = RISA_DENSE_VALUE((RisaDenseValue*) newStr);

                                        risa_gc_check(vm);
                                    }
                                }

                                goto _op_set_success;
                            }*/
                            case RISA_DVAL_OBJECT: {
                                if(!risa_value_is_dense_of_type(LEFT_BY_TYPE, RISA_DVAL_STRING)) {
                                    VM_RUNTIME_ERROR(vm, "Object key must be string");
                                    return RISA_VM_STATUS_ERROR;
                                }

                                RisaDenseObject* object = RISA_AS_OBJECT(DEST_REG);
                                RisaDenseString* key = RISA_AS_STRING(LEFT_BY_TYPE);

                                risa_dense_object_set(object, key, RIGHT_BY_TYPE);

                                risa_gc_check(vm);

                                goto _op_set_success;
                            }
                            default:
                                break;
                        }
                        // Fallthrough
                    default:
                        VM_RUNTIME_ERROR(vm, "Left operand must be an array, string, or object");
                        return RISA_VM_STATUS_ERROR;
                }

            _op_set_success:

                SKIP(3);
                break;
            }
            case RISA_OP_NULL: {
                DEST_REG = RISA_NULL_VALUE;

                SKIP(3);
                break;
            }
            case RISA_OP_TRUE: {
                DEST_REG = RISA_BOOL_VALUE(true);

                SKIP(3);
                break;
            }
            case RISA_OP_FALSE: {
                DEST_REG = RISA_BOOL_VALUE(false);

                SKIP(3);
                break;
            }
            case RISA_OP_NOT: {
                DEST_REG = RISA_BOOL_VALUE(risa_value_is_falsy(LEFT_BY_TYPE));

                SKIP(3);
                break;
            }
            case RISA_OP_BNOT: {
                RisaValue left = LEFT_BY_TYPE;

                if(RISA_IS_BYTE(left))
                    DEST_REG = RISA_BYTE_VALUE(~RISA_AS_BYTE(left));
                else if(RISA_IS_INT(left))
                    DEST_REG = RISA_INT_VALUE(~RISA_AS_INT(left));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_NEG: {
                RisaValue left = LEFT_BY_TYPE;

                if(RISA_IS_BYTE(left))
                    DEST_REG = RISA_INT_VALUE(-((int64_t) RISA_AS_BYTE(left)));
                else if(RISA_IS_INT(left))
                    DEST_REG = RISA_INT_VALUE(-RISA_AS_INT(left));
                else if(RISA_IS_FLOAT(left))
                    DEST_REG = RISA_FLOAT_VALUE(-RISA_AS_FLOAT(left));
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_INC: {
                RisaValue dest = DEST_REG;

                if(RISA_IS_BYTE(dest))
                    ++RISA_AS_BYTE(DEST_REG);
                else if(RISA_IS_INT(dest))
                    ++RISA_AS_INT(DEST_REG);
                else if(RISA_IS_FLOAT(dest))
                    ++RISA_AS_FLOAT(DEST_REG);
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_DEC: {
                RisaValue dest = DEST_REG;

                if(RISA_IS_BYTE(dest))
                    --RISA_AS_BYTE(DEST_REG);
                else if(RISA_IS_INT(dest))
                    --RISA_AS_INT(DEST_REG);
                else if(RISA_IS_FLOAT(dest))
                    --RISA_AS_FLOAT(DEST_REG);
                else {
                    VM_RUNTIME_ERROR(vm, "Operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_ADD: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) + RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) + RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_BYTE(left) + RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) + RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) + RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_INT(left) + RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) + RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) + RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) + RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(risa_value_is_dense_of_type(left, RISA_DVAL_STRING)) {
                    if(risa_value_is_dense_of_type(right, RISA_DVAL_STRING)) {
                        RisaDenseString* result = risa_dense_string_concat(RISA_AS_STRING(left), RISA_AS_STRING(right));
                        RisaDenseString* interned = risa_map_find(&vm->strings, result->chars, result->length, result->hash);

                        if(interned != NULL) {
                            RISA_MEM_FREE(result);
                            result = interned;
                            DEST_REG = RISA_DENSE_VALUE(result);
                        } else {
                            risa_vm_register_string(vm, result);
                            risa_vm_register_dense(vm, (RisaDenseValue *) result);
                            DEST_REG = RISA_DENSE_VALUE(result);
                            risa_gc_check(vm);
                        }
                    } else {
                        VM_RUNTIME_ERROR(vm, "Left operand must be string");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int, float or string");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_SUB: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) - RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) - RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_BYTE(left) - RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) - RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) - RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_INT(left) - RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) - RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) - RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) - RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_MUL: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) * RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) * RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_BYTE(left) * RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) * RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) * RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_INT(left) * RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) * RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) * RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) * RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_DIV: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) / RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) / RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_BYTE(left) / RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) / RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) / RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_INT(left) / RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) / RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) / RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_FLOAT_VALUE(RISA_AS_FLOAT(left) / RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte, int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte, int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_MOD: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) % RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) % RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_INT(left) % RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) % RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_SHL: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) << RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        if(RISA_AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return RISA_VM_STATUS_ERROR;
                        }

                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) << RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_AS_INT(left) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return RISA_VM_STATUS_ERROR;
                    }

                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) << RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        if(RISA_AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift left with a negative amount");
                            return RISA_VM_STATUS_ERROR;
                        }

                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) << RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_SHR: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) >> RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        if(RISA_AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return RISA_VM_STATUS_ERROR;
                        }

                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) >> RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_AS_INT(left) < 0) {
                        VM_RUNTIME_ERROR(vm, "Cannot shift negative numbers");
                        return RISA_VM_STATUS_ERROR;
                    }

                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) >> RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        if(RISA_AS_INT(right) < 0) {
                            VM_RUNTIME_ERROR(vm, "Cannot shift right with a negative amount");
                            return RISA_VM_STATUS_ERROR;
                        }

                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) >> RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_LT: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) < RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) < RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) < RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) < RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) < RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) < RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) < RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) < RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) < RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_LTE: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) <= RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) <= RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_BYTE(left) <= RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) <= RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) <= RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_INT(left) <= RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_FLOAT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) <= RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) <= RISA_AS_INT(right));
                    } else if(RISA_IS_FLOAT(right)) {
                        DEST_REG = RISA_BOOL_VALUE(RISA_AS_FLOAT(left) <= RISA_AS_FLOAT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either int or float");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either int or float");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_EQ: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                DEST_REG = RISA_BOOL_VALUE(risa_value_equals(left, right));

                SKIP(3);
                break;
            }
            case RISA_OP_NEQ: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                DEST_REG = RISA_BOOL_VALUE(!risa_value_equals(left, right));

                SKIP(3);
                break;
            }
            case RISA_OP_BAND: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) & RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) & RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) & RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) & RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_BXOR: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) ^ RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) ^ RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) ^ RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) ^ RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_BOR: {
                RisaValue left = LEFT_BY_TYPE;
                RisaValue right = RIGHT_BY_TYPE;

                if(RISA_IS_BYTE(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_BYTE_VALUE(RISA_AS_BYTE(left) | RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_BYTE(left) | RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else if(RISA_IS_INT(left)) {
                    if(RISA_IS_BYTE(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) | RISA_AS_BYTE(right));
                    } else if(RISA_IS_INT(right)) {
                        DEST_REG = RISA_INT_VALUE(RISA_AS_INT(left) | RISA_AS_INT(right));
                    } else {
                        VM_RUNTIME_ERROR(vm, "Right operand must be either byte or int");
                        return RISA_VM_STATUS_ERROR;
                    }
                } else {
                    VM_RUNTIME_ERROR(vm, "Left operand must be either byte or int");
                    return RISA_VM_STATUS_ERROR;
                }

                SKIP(3);
                break;
            }
            case RISA_OP_TEST: {
                if(risa_value_is_truthy(DEST_REG))
                    SKIP(4);
                SKIP(3);
                break;
            }
            case RISA_OP_NTEST: {
                if(risa_value_is_falsy(DEST_REG))
                    SKIP(4);
                SKIP(3);
                break;
            }
            case RISA_OP_JMP: {
                SKIP(DEST * 4);
                SKIP(3);
                break;
            }
            case RISA_OP_JMPW: {
                uint16_t amount = *((uint16_t*) &frame->ip); // This takes DEST and LEFT as 16 bits.

                SKIP(amount * 4);
                SKIP(3);
                break;
            }
            case RISA_OP_BJMP: {
                BSKIP(DEST * 4);
                BSKIP(1);
                break;
            }
            case RISA_OP_BJMPW: {
                uint16_t amount = *((uint16_t*) &frame->ip); // This takes DEST and LEFT as 16 bits.

                BSKIP(amount * 4);
                BSKIP(1);
                break;
            }
            case RISA_OP_CALL: {
                if(!risa_vm_call_register(vm, DEST, LEFT))
                    return RISA_VM_STATUS_ERROR;

                // Native function.
                if(frame == &vm->frames[vm->frameCount - 1])
                    SKIP(3);
                else frame = &vm->frames[vm->frameCount - 1];

                break;
            }
            case RISA_OP_RET: {
                risa_vm_upvalue_close_from(vm, frame->regs);

                --vm->frameCount;

                // When returning from the first frame, halt the RisaVM.
                if(vm->frameCount == 0)
                    return RISA_VM_STATUS_OK;

                // The reg that contained the function will now contain the returned value.
                // This has to be done manually for native functions (see risa_vm_call_native).
                if(DEST > 249)
                    *frame->base = RISA_NULL_VALUE;
                else *frame->base = DEST_REG;

                vm->stackTop -= 251; // = frame->base + 1;

                frame = &vm->frames[vm->frameCount - 1];

                // If the frame is isolated, halt the RisaVM.
                if(vm->frames[vm->frameCount].isolated) {
                    return RISA_VM_STATUS_OK;
                }

                SKIP(3); // Skip the CALL args.
                break;
            }
            case RISA_OP_ACC: {
                vm->acc = DEST_BY_TYPE;

                SKIP(3);
                break;
            }
            case RISA_OP_DIS: {
                RisaDenseFunction* func;

                if(DEST > 249)
                    func = VM_FRAME_FUNCTION(*frame);
                else {
                    if(risa_value_is_dense_of_type(DEST_REG, RISA_DVAL_FUNCTION))
                        func = RISA_AS_FUNCTION(DEST_REG);
                    else if(risa_value_is_dense_of_type(DEST_REG, RISA_DVAL_CLOSURE))
                        func = RISA_AS_CLOSURE(DEST_REG)->function;
                    else {
                        VM_RUNTIME_ERROR(vm, "Argument must be a non-native function");
                        return RISA_VM_STATUS_ERROR;
                    }
                }

                RISA_OUT(vm->io, "\n");
                risa_dense_print(&vm->io, (RisaDenseValue *) func);

                RisaDisassembler disasm;
                risa_disassembler_init(&disasm);
                risa_io_clone(&disasm.io, &vm->io);

                risa_disassembler_load(&disasm, &func->cluster);
                risa_disassembler_run(&disasm);
                RISA_OUT(vm->io, "\n\n");

                SKIP(3);
                break;
            }
            default: {
                VM_RUNTIME_ERROR(vm, "Illegal instruction");
                return RISA_VM_STATUS_ERROR;
            }
        }

        if(forever) {
            continue;
        }

        if(--maxInstr == 0) {
            break;
        }
    }

    return RISA_VM_STATUS_HALTED;

    #undef BSKIP
    #undef SKIP

    #undef RIGHT_BY_TYPE
    #undef LEFT_BY_TYPE

    #undef RIGHT_REG
    #undef LEFT_REG
    #undef DEST_REG

    #undef COMBINED_CONST
    #undef RIGHT_CONST
    #undef LEFT_CONST
    #undef DEST_CONST

    #undef COMBINED
    #undef RIGHT
    #undef LEFT
    #undef DEST

    #undef NEXT_CONSTANT
    #undef NEXT_BYTE
}

RisaValue risa_vm_invoke(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, ...) {
    // Check the frame count before pushing anything on the stack.
    if(vm->frameCount == RISA_VM_CALLFRAME_COUNT) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return RISA_NULL_VALUE;
    }

    RisaValue* ptr = base + 1;
    RisaValue* end = ptr + argc;

    va_list args;

    va_start(args, argc);

    // Push the args on the stack.
    while(ptr < end) {
        *ptr++ = va_arg(args, RisaValue);
    }

    va_end(args);

    return risa_vm_invoke_directly(vm, base, callee, argc);
}

RisaValue risa_vm_invoke_args(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, RisaValue* args) {
    // Check the frame count before pushing anything on the stack.
    if(vm->frameCount == RISA_VM_CALLFRAME_COUNT) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return RISA_NULL_VALUE;
    }

    RisaValue* ptr = base + 1;
    RisaValue* end = ptr + argc;

    // Push the args on the stack.
    while(ptr < end) {
        *ptr++ = *args++;
    }

    return risa_vm_invoke_directly(vm, base, callee, argc);
}

static RisaValue risa_vm_invoke_directly(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc) {
    if(RISA_IS_DENSE(callee)) {
        switch(RISA_AS_DENSE(callee)->type) {
            case RISA_DVAL_FUNCTION: {
                if(!risa_vm_call_function(vm, base, callee, argc, true))
                    return RISA_NULL_VALUE;

                goto _vm_invoke_directly_run;
            }
            case RISA_DVAL_CLOSURE: {
                if(!risa_vm_call_closure(vm, base, callee, argc, true))
                    return RISA_NULL_VALUE;

            _vm_invoke_directly_run:

                if(risa_vm_run(vm, 0) == RISA_VM_STATUS_ERROR)
                    return RISA_NULL_VALUE;

                return *base;
            }
            case RISA_DVAL_NATIVE: {
                if(!risa_vm_call_native(vm, base, callee, argc, true))
                    return RISA_NULL_VALUE;

                return *base;
            }
            default: ;
        }
    }

    VM_RUNTIME_ERROR(vm, "Cannot call non-function type");
    return RISA_NULL_VALUE;
}

static bool risa_vm_call_register(RisaVM* vm, uint8_t reg, uint8_t argc) {
    RisaValue* callee = &vm->frames[vm->frameCount - 1].regs[reg];

    return risa_vm_call_value(vm, callee, *callee, argc, false);
}

static bool risa_vm_call_value(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, bool isolated) {
    if(RISA_IS_DENSE(callee)) {
        switch(RISA_AS_DENSE(callee)->type) {
            case RISA_DVAL_FUNCTION:
                return risa_vm_call_function(vm, base, callee, argc, isolated);
            case RISA_DVAL_CLOSURE:
                return risa_vm_call_closure(vm, base, callee, argc, isolated);
            case RISA_DVAL_NATIVE:
                return risa_vm_call_native(vm, base, callee, argc, isolated);
            default: ;
        }
    }

    VM_RUNTIME_ERROR(vm, "Cannot call non-function type");
    return false;
}

static bool risa_vm_call_function(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, bool isolated) {
    RisaDenseFunction* function = RISA_AS_FUNCTION(callee);

    if(argc != function->arity) {
        VM_RUNTIME_ERROR(vm, "Expected %x args, got %x", function->arity, argc);
        return false;
    }

    if(vm->frameCount == RISA_VM_CALLFRAME_COUNT) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return false;
    }

    ++vm->frameCount;

    vm->frames[vm->frameCount - 1] = risa_vm_frame_from_function(vm, base, function, isolated);
    vm->stackTop += 251;

    return true;
}

static bool risa_vm_call_closure(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, bool isolated) {
    RisaDenseClosure* closure = RISA_AS_CLOSURE(callee);
    RisaDenseFunction* function = closure->function;

    if(argc != function->arity) {
        VM_RUNTIME_ERROR(vm, "Expected %x args, got %x", function->arity, argc);
        return false;
    }

    if(vm->frameCount == RISA_VM_CALLFRAME_COUNT) {
        VM_RUNTIME_ERROR(vm, "Stack overflow");
        return false;
    }

    ++vm->frameCount;

    vm->frames[vm->frameCount - 1] = risa_vm_frame_from_closure(vm, base, closure, isolated);
    vm->stackTop += 251;

    return true;
}

static bool risa_vm_call_native(RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, bool isolated) {
    RisaDenseNative* native = RISA_AS_NATIVE(callee);

    *base = native->function(vm, argc, base + 1);

    return true;
}

static RisaDenseUpvalue* risa_vm_upvalue_capture(RisaVM* vm, RisaValue* local) {
    RisaDenseUpvalue* prev = NULL;
    RisaDenseUpvalue* upvalue = vm->upvalues;

    while(upvalue != NULL && upvalue->ref > local) {
        prev = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->ref == local)
        return upvalue;

    RisaDenseUpvalue* created = risa_dense_upvalue_create(local);
    created->next = upvalue;

    if(prev == NULL)
        vm->upvalues = created;
    else prev->next = created;

    risa_vm_register_dense(vm, (RisaDenseValue*) created);

    return created;
}

static void risa_vm_upvalue_close_from(RisaVM* vm, RisaValue* slot) {
    while(vm->upvalues != NULL && vm->upvalues->ref >= slot) {
        RisaDenseUpvalue* head = vm->upvalues;
        head->closed = *head->ref;
        head->ref = &head->closed;
        vm->upvalues = head->next;
    }
}

#undef VM_RUNTIME_ERROR