#include "risa.h"

#include "chunk/chunk.h"
#include "compiler/compiler.h"
#include "asm/disassembler.h"
#include "io/log.h"
#include "def/def.h"

RisaCompileStatus risa_compile_string(Compiler* compiler, const char* str) {
    if(compiler_compile(compiler, str) == COMPILER_ERROR)
        return RISA_COMPILE_ERROR;

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
    vm_clean(vm);

    vm->frames[0] = vm_frame_from_function(vm, NULL, function, true);
    vm->frameCount = 1;
    vm->stackTop += 250;

    vm_register_dense(vm, (DenseValue*) function);

    if(vm_execute(vm) == VM_ERROR)
        return RISA_EXECUTE_ERROR;
    return RISA_EXECUTE_OK;
}

RisaInterpretStatus risa_interpret_string(VM* vm, const char* str) {
    Compiler compiler;
    compiler_init(&compiler);

    compiler.strings = vm->strings;
    compiler.options = vm->options;

    if(risa_compile_string(&compiler, str) == RISA_COMPILE_ERROR) {
        //compiler_delete(&compiler);
        chunk_delete(&compiler.function->chunk);
        RISA_MEM_FREE(compiler.function);

        return RISA_INTERPRET_COMPILE_ERROR;
    }

    vm->strings = compiler.strings;

    /*for(uint32_t i = 0; i < compiler.strings.capacity; ++i) {
        DenseString* string = compiler.strings.entries[i].key;

        if(string != NULL)
            vm_register_string(vm, string);
    }*/

    if(risa_execute_function(vm, compiler.function) == RISA_EXECUTE_ERROR) {
        //compiler_delete(&compiler);
        //vm_delete(vm);

        return RISA_INTERPRET_EXECUTE_ERROR;
    }

    //compiler_delete(&compiler);
    //vm_delete(&vm);

    return RISA_INTERPRET_OK;
}