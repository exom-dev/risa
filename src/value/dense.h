#ifndef RISA_DENSE_H
#define RISA_DENSE_H

#include "../common/headers.h"
#include "../chunk/chunk.h"
#include "../memory/mem.h"

typedef Value (*NativeFunction)(void* vm, uint8_t argc, Value* args);

typedef struct {
    DenseValue dense;

    uint32_t length;
    uint32_t hash;

    char chars[];
} DenseString;

typedef struct DenseUpvalue {
    DenseValue dense;

    Value* ref;
    Value  closed;

    struct DenseUpvalue* next;
} DenseUpvalue;

typedef struct {
    DenseValue dense;

    uint8_t arity;

    Chunk chunk;
    DenseString* name;
} DenseFunction;

typedef struct {
    DenseValue dense;

    DenseFunction* function;
    DenseUpvalue** upvalues;

    uint8_t upvalueCount;
} DenseClosure;

typedef struct {
    DenseValue dense;

    NativeFunction function;
} DenseNative;

#define AS_STRING(value)   ((DenseString*) ((value).as.dense))
#define AS_CSTRING(value)  (((DenseString*) ((value).as.dense))->chars)
#define AS_FUNCTION(value) ((DenseFunction*) ((value).as.dense))
#define AS_CLOSURE(value)  ((DenseClosure*) ((value).as.dense))
#define AS_NATIVE(value)   ((DenseNative*) ((value).as.dense))

void   dense_print(DenseValue* dense);
size_t dense_size(DenseValue* dense);
void   dense_delete(DenseValue* dense);

uint32_t     dense_string_hash(DenseString* string);
DenseString* dense_string_from(const char* chars, uint32_t length);
DenseString* dense_string_concat(DenseString* left, DenseString* right);

DenseUpvalue* dense_upvalue_create(Value* value);

DenseFunction* dense_function_create();
void           dense_function_init(DenseFunction* function);
void           dense_function_delete(DenseFunction* function);

DenseClosure* dense_closure_create(DenseFunction* function, uint8_t upvalueCount);

DenseNative* dense_native_create(NativeFunction function);

#endif
