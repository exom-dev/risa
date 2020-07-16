#include "risa.h"

#include "chunk/chunk.h"
#include "compiler/compiler.h"
#include "debug/disassembler.h"
#include "common/logging.h"
#include "common/def.h"

RisaCompileStatus risa_compile_string(const char* str, Chunk* chunk) {
    Compiler compiler;

    compiler_init(&compiler);

    if(compiler_compile(&compiler, str) == COMPILER_ERROR)
        return  RISA_COMPILE_ERROR;

    *chunk = compiler.function->chunk;

    #ifdef DEBUG_SHOW_DISASSEMBLY
        PRINT("\n<script>");
        debug_disassemble_chunk(chunk);

        PRINT("\n");

        for(uint32_t i = 0; i < chunk->constants.size; ++i) {
            if(value_is_dense_of_type(chunk->constants.values[i], DVAL_FUNCTION)) {
                PRINT("<%s>", AS_FUNCTION(chunk->constants.values[i])->name->chars);
                debug_disassemble_chunk(&AS_FUNCTION(chunk->constants.values[i])->chunk);
                PRINT("\n");
            }
        }
    #endif

    compiler_delete(&compiler);

    return RISA_COMPILE_OK;
}

RisaExecuteStatus risa_execute_chunk(VM* vm, Chunk chunk) {
    DenseFunction* function = dense_function_create();

    function->arity = 0;
    function->name = NULL;
    function->chunk = chunk;

    RisaExecuteStatus status = risa_execute_function(vm, function);

    dense_function_delete(function);
    MEM_FREE(function);

    return status;
}

RisaExecuteStatus risa_execute_function(VM* vm, DenseFunction* function) {
    CallFrame frame;

    frame.function = function;
    frame.ip = frame.function->chunk.bytecode;
    frame.base = vm->stackTop++;
    frame.regs = frame.base + 1;

    vm->frames[0] = frame;
    vm->frameCount = 1;

    if(vm_execute(vm) == VM_ERROR)
        return RISA_EXECUTE_ERROR;
    return RISA_EXECUTE_OK;
}

RisaInterpretStatus risa_interpret_string(const char* str) {
    Chunk compiled;
    chunk_init(&compiled);

    if(risa_compile_string(str, &compiled) == RISA_COMPILE_ERROR) {
        chunk_delete(&compiled);
        return RISA_INTERPRET_COMPILE_ERROR;
    }

    VM vm;
    vm_init(&vm);

    for(uint32_t i = 0; i < compiled.constants.size; ++i) {
        if(IS_DENSE(compiled.constants.values[i])) {
            vm_register_value(&vm, AS_DENSE(compiled.constants.values[i]));

            if(AS_DENSE(compiled.constants.values[i])->type == DVAL_STRING)
                vm_register_string(&vm, AS_STRING(compiled.constants.values[i]));
        }
    }

    if(risa_execute_chunk(&vm, compiled) == RISA_EXECUTE_ERROR) {
        vm_delete(&vm);
        chunk_delete(&compiled);
        return RISA_INTERPRET_EXECUTE_ERROR;
    }

    vm_delete(&vm);
    chunk_delete(&compiled);

    return RISA_INTERPRET_OK;
}