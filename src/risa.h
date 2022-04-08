#ifndef RISA_RISA_H_GUARD
#define RISA_RISA_H_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#include "api.h"
#include "vm/vm.h"
#include "std/std.h"
#include "compiler/compiler.h"
#include "version.h"

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

RISA_API RisaCompileStatus   risa_compile_string    (RisaCompiler* compiler, const char* str);
RISA_API RisaExecuteStatus   risa_execute_cluster   (RisaVM* vm, RisaCluster cluster);
RISA_API RisaExecuteStatus   risa_execute_function  (RisaVM* vm, RisaDenseFunction* function);
RISA_API RisaInterpretStatus risa_interpret_string  (RisaVM* vm, const char* str);
RISA_API uint8_t*            risa_serialize_cluster (RisaCluster* cluster, uint32_t* size);

#ifdef __cplusplus
}
#endif

#endif
