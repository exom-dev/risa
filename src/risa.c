#include "risa.h"

#include "cluster/cluster.h"
#include "compiler/compiler.h"
#include "asm/disassembler.h"
#include "io/log.h"
#include "def/def.h"

RisaCompileStatus risa_compile_string(RisaCompiler* compiler, const char* str) {
    if(risa_compiler_compile(compiler, str) == RISA_COMPILER_STATUS_ERROR)
        return RISA_COMPILE_ERROR;

    return RISA_COMPILE_OK;
}

RisaExecuteStatus risa_execute_cluster(RisaVM* vm, RisaCluster cluster) {
    RisaDenseFunction* function = risa_dense_function_create();

    function->arity = 0;
    function->name = NULL;
    function->cluster = cluster;

    RisaExecuteStatus status = risa_execute_function(vm, function);

    return status;
}

RisaExecuteStatus risa_execute_function(RisaVM* vm, RisaDenseFunction* function) {
    risa_vm_load_function(vm, function);

    if(risa_vm_execute(vm) == RISA_VM_STATUS_ERROR)
        return RISA_EXECUTE_ERROR;
    return RISA_EXECUTE_OK;
}

RisaInterpretStatus risa_interpret_string(RisaVM* vm, const char* str) {
    RisaCompiler compiler;

    risa_compiler_init(&compiler);
    risa_compiler_target(&compiler, vm);

    if(risa_compile_string(&compiler, str) == RISA_COMPILE_ERROR) {
        //risa_compiler_delete(&compiler);
        risa_cluster_delete(&compiler.function->cluster);
        RISA_MEM_FREE(compiler.function);

        return RISA_INTERPRET_COMPILE_ERROR;
    }

    risa_vm_load_compiler_data(vm, &compiler);

    /*for(uint32_t i = 0; i < compiler.strings.capacity; ++i) {
        RisaDenseString* string = compiler.strings.entries[i].key;

        if(string != NULL)
            risa_vm_register_string(vm, string);
    }*/

    if(risa_execute_function(vm, compiler.function) == RISA_EXECUTE_ERROR) {
        //risa_compiler_delete(&compiler);
        //risa_vm_delete(vm);

        return RISA_INTERPRET_EXECUTE_ERROR;
    }

    //risa_compiler_delete(&compiler);
    //risa_vm_delete(&vm);

    return RISA_INTERPRET_OK;
}