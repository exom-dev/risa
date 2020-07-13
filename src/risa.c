#include "risa.h"

#include "chunk/chunk.h"
#include "compiler/compiler.h"
#include "debug/disassembler.h"
#include "common/logging.h"

RisaCompileStatus risa_compile_string(const char* str, Chunk* chunk) {
    Compiler compiler;

    compiler_init(&compiler);

    if(compiler_compile(&compiler, str) == COMPILER_ERROR)
        return  RISA_COMPILE_ERROR;

    *chunk = compiler.chunk;

    debug_disassemble_chunk(chunk);

    return RISA_COMPILE_OK;
}

RisaExecuteStatus risa_execute_chunk(VM* vm, Chunk* chunk) {
    vm->chunk = chunk;
    vm->ip = chunk->bytecode;

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

    for(uint32_t i = 0; i < compiled.constants.size; ++i)
        if(IS_LINKED(compiled.constants.values[i]))
            vm_register_value(&vm, AS_LINKED(compiled.constants.values[i]));

    if(risa_execute_chunk(&vm, &compiled) == RISA_EXECUTE_ERROR) {
        vm_delete(&vm);
        chunk_delete(&compiled);
        return RISA_INTERPRET_EXECUTE_ERROR;
    }

    vm_delete(&vm);
    chunk_delete(&compiled);

    return RISA_INTERPRET_OK;
}