#ifndef RISA_RISA_H_GUARD
#define RISA_RISA_H_GUARD

#include "vm/vm.h"
#include "compiler/compiler.h"

#define RISA_VERSION "0.1 ALPHA"
#define RISA_CODENAME "Initus"

typedef enum {
    RISA_COMPILE_OK,
    RISA_COMPILE_ERROR
} RisaCompileStatus;

typedef enum {
    RISA_EXECUTE_OK,
    RISA_EXECUTE_ERROR
} RisaExecuteStatus;

typedef enum {
    RISA_INTERPRET_OK,
    RISA_INTERPRET_COMPILE_ERROR,
    RISA_INTERPRET_EXECUTE_ERROR
} RisaInterpretStatus;

RisaCompileStatus risa_compile_string(Compiler* compiler, const char* str);
RisaExecuteStatus risa_execute_chunk(VM* vm, Chunk chunk);
RisaExecuteStatus risa_execute_function(VM* vm, DenseFunction* function);
RisaInterpretStatus risa_interpret_string(const char* str);

#endif
