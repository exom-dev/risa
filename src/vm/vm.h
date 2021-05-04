#ifndef RISA_VM_H_GUARD
#define RISA_VM_H_GUARD

#include "../api.h"
#include "../io/io.h"
#include "../cluster/cluster.h"
#include "../def/def.h"
#include "../data/map.h"
#include "../value/dense.h"
#include "../options/options.h"

// This is also used in 'mem/gc.c'
#define VM_FRAME_FUNCTION(frame) ((frame).type == RISA_FRAME_FUNCTION ? (frame).callee.function : (frame).callee.closure->function)

typedef enum {
    RISA_FRAME_FUNCTION,
    RISA_FRAME_CLOSURE
} RisaCallFrameType;

typedef struct {
    RisaCallFrameType type;

    union {
        RisaDenseFunction* function;
        RisaDenseClosure* closure;
    } callee;

    uint8_t* ip;

    RisaValue* base;
    RisaValue* regs;

    bool isolated;
} RisaCallFrame;

typedef struct {
    RisaIO io;
    RisaCallFrame frames[RISA_VM_CALLFRAME_COUNT];
    uint32_t frameCount;

    RisaValue  stack[RISA_VM_STACK_SIZE];
    RisaValue* stackTop;

    RisaMap strings;
    RisaMap globals;

    RisaDenseValue* values;
    RisaDenseUpvalue* upvalues;

    RisaOptions options;

    RisaValue acc; // The accumulator, used in REPL mode to store the lastReg value.

    size_t heapSize;
    size_t heapThreshold;
} RisaVM;

typedef enum {
    RISA_VM_STATUS_OK,
    RISA_VM_STATUS_ERROR
} RisaVMStatus;

RISA_API RisaVM*          risa_vm_create                   ();
RISA_API void             risa_vm_init                     (RisaVM* vm);
RISA_API void             risa_vm_clean                    (RisaVM* vm);
RISA_API void             risa_vm_load_compiler_data       (RisaVM* vm, void* compiler); // Avoid including the compiler in the header.
RISA_API void             risa_vm_load_function            (RisaVM* vm, RisaDenseFunction* function);
RISA_API void             risa_vm_load_strings             (RisaVM* vm, RisaMap* strings);
RISA_API RisaMap*         risa_vm_get_strings              (RisaVM* vm);
RISA_API RisaIO*          risa_vm_get_io                   (RisaVM* vm);
RISA_API RisaValue        risa_vm_get_acc                  (RisaVM* vm);
RISA_API void             risa_vm_set_repl_mode            (RisaVM* vm, bool mode);
RISA_API void             risa_vm_delete                   (RisaVM* vm);
RISA_API void             risa_vm_free                     (RisaVM* vm);

RISA_API RisaVMStatus     risa_vm_execute                  (RisaVM* vm);
RISA_API RisaVMStatus     risa_vm_run                      (RisaVM* vm);
RISA_API RisaValue        risa_vm_invoke                   (RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, ...);
RISA_API RisaValue        risa_vm_invoke_args              (RisaVM* vm, RisaValue* base, RisaValue callee, uint8_t argc, RisaValue* args);

RISA_API void             risa_vm_register_string          (RisaVM* vm, RisaDenseString* string);
RISA_API void             risa_vm_register_dense           (RisaVM* vm, RisaDenseValue* dense);
RISA_API void             risa_vm_register_dense_unchecked (RisaVM* vm, RisaDenseValue* dense);

// isolated = upon returning from this frame, halt the VM. Useful for the first frame,
// and for calling other functions from native functions.
RISA_API RisaCallFrame    risa_vm_frame_from_function      (RisaVM* vm, RisaValue* base, RisaDenseFunction* function, bool isolated);
RISA_API RisaCallFrame    risa_vm_frame_from_closure       (RisaVM* vm, RisaValue* base, RisaDenseClosure* closure, bool isolated);

RISA_API RisaDenseString* risa_vm_string_create            (RisaVM* vm, const char* str, uint32_t length);
RISA_API RisaDenseString* risa_vm_string_internalize       (RisaVM* vm, RisaDenseString* str);

RISA_API void             risa_vm_global_set               (RisaVM* vm, const char* str, uint32_t length, RisaValue value);
RISA_API void             risa_vm_global_set_native        (RisaVM* vm, const char* str, uint32_t length, RisaNativeFunction fn);

RISA_API void             risa_vm_stack_reset              (RisaVM* vm);
RISA_API void             risa_vm_stack_push               (RisaVM* vm, RisaValue value);
RISA_API RisaValue        risa_vm_stack_pop                (RisaVM* vm);
RISA_API RisaValue        risa_vm_stack_peek               (RisaVM* vm, size_t range);

#endif
