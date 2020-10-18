#include "risa.h"

#include "chunk/chunk.h"
#include "compiler/compiler.h"
#include "asm/disassembler.h"
#include "common/logging.h"
#include "common/def.h"

RisaCompileStatus risa_compile_string(Compiler* compiler, const char* str) {
    if(compiler_compile(compiler, str) == COMPILER_ERROR)
        return  RISA_COMPILE_ERROR;

    #ifdef DEBUG_SHOW_DISASSEMBLY
        PRINT("\n<script>");
        disassembler_process_chunk(&compiler->function->chunk);
        PRINT("\n");
    #endif

    return RISA_COMPILE_OK;
}

RisaExecuteStatus risa_execute_chunk(VM* vm, Chunk chunk) {
    DenseFunction* function = dense_function_create();

    function->arity = 0;
    function->name = NULL;
    function->chunk = chunk;

    RisaExecuteStatus status = risa_execute_function(vm, function);

    return status;
}

RisaExecuteStatus risa_execute_function(VM* vm, DenseFunction* function) {
    CallFrame frame;

    frame.type = FRAME_FUNCTION;
    frame.callee.function = function;
    frame.ip = frame.callee.function->chunk.bytecode;
    frame.base = vm->stackTop++;
    frame.regs = frame.base + 1;

    vm->frames[0] = frame;
    vm->frameCount = 1;
    vm->stackTop += 250;

    vm_register_dense(vm, (DenseValue*) function);

    if(vm_execute(vm) == VM_ERROR)
        return RISA_EXECUTE_ERROR;
    return RISA_EXECUTE_OK;
}

Value print(void* vm, uint8_t argc, Value* args) {
    value_print(args[0]);
    return args[0];
}

RisaInterpretStatus risa_interpret_string(const char* str) {
    Compiler compiler;
    compiler_init(&compiler);

    if(risa_compile_string(&compiler, str) == RISA_COMPILE_ERROR) {
        compiler_delete(&compiler);
        chunk_delete(&compiler.function->chunk);
        MEM_FREE(compiler.function);

        return RISA_INTERPRET_COMPILE_ERROR;
    }

    VM vm;
    vm_init(&vm);

    for(uint32_t i = 0; i < compiler.strings.capacity; ++i) {
        DenseString* string = compiler.strings.entries[i].key;

        if(string != NULL)
            vm_register_string(&vm, string);
    }

    DenseNative* native = dense_native_create(print);
    DenseString* string = map_find(&vm.strings, "print", 5, map_hash("print", 5));

    if(string == NULL) {
        string = dense_string_from("print", 5);
        vm_register_string(&vm, string);
        vm_register_dense(&vm, (DenseValue*) string);
    }

    map_set(&vm.globals, string, DENSE_VALUE(native));
    vm_register_dense(&vm, (DenseValue*) native);

    if(risa_execute_function(&vm, compiler.function) == RISA_EXECUTE_ERROR) {
        compiler_delete(&compiler);
        vm_delete(&vm);

        #ifdef DEBUG_SHOW_HEAP_SIZE
            PRINT("\n\nHeap size: %zu\n", vm.heapSize);
        #endif

        return RISA_INTERPRET_EXECUTE_ERROR;
    }

    compiler_delete(&compiler);
    vm_delete(&vm);

    #ifdef DEBUG_SHOW_HEAP_SIZE
        PRINT("\n\nHeap size: %zu\n", vm.heapSize);
    #endif

    return RISA_INTERPRET_OK;
}