#ifndef RISA_DENSE_H
#define RISA_DENSE_H

#include "../io/io.h"
#include "../def/types.h"
#include "../cluster/cluster.h"
#include "../memory/mem.h"
#include "../data/map.h"

typedef RisaValue (*RisaNativeFunction)(void* vm, uint8_t argc, RisaValue* args);

typedef struct {
    RisaDenseValue dense;

    uint32_t length;
    uint32_t hash;

    char chars[];
} RisaDenseString;

typedef struct {
    RisaDenseValue dense;

    RisaValueArray data;
} RisaDenseArray;

typedef struct {
    RisaDenseValue dense;

    RisaMap data;
} RisaDenseObject;

typedef struct RisaDenseUpvalue {
    RisaDenseValue dense;

    RisaValue* ref;
    RisaValue  closed;

    struct RisaDenseUpvalue* next;
} RisaDenseUpvalue;

typedef struct {
    RisaDenseValue dense;

    uint8_t arity;

    RisaCluster cluster;
    RisaDenseString* name;
} RisaDenseFunction;

typedef struct {
    RisaDenseValue dense;

    RisaDenseFunction* function;
    RisaDenseUpvalue** upvalues;

    uint8_t upvalueCount;
} RisaDenseClosure;

typedef struct {
    RisaDenseValue dense;

    RisaNativeFunction function;
} RisaDenseNative;

#define AS_STRING(value)   ((RisaDenseString*) ((value).as.dense))
#define AS_ARRAY(value)    ((RisaDenseArray*) ((value).as.dense))
#define AS_OBJECT(value)   ((RisaDenseObject*) ((value).as.dense))
#define AS_CSTRING(value)  (((RisaDenseString*) ((value).as.dense))->chars)
#define AS_FUNCTION(value) ((RisaDenseFunction*) ((value).as.dense))
#define AS_CLOSURE(value)  ((RisaDenseClosure*) ((value).as.dense))
#define AS_NATIVE(value)   ((RisaDenseNative*) ((value).as.dense))

void               risa_dense_print               (RisaIO* io, RisaDenseValue* dense);
char*              risa_dense_to_string           (RisaDenseValue* dense);
bool               risa_dense_is_truthy           (RisaDenseValue* dense);
RisaValue          risa_dense_clone               (RisaDenseValue* dense);
RisaValue          risa_dense_clone_under         (void* vm, RisaDenseValue* dense); // void* instead of RisaVM* in order to work around the circular dependency.
size_t             risa_dense_size                (RisaDenseValue* dense);
void               risa_dense_delete              (RisaDenseValue* dense);

RisaDenseString*   risa_dense_string_prepare      (const char* chars, uint32_t length);
uint32_t           risa_dense_string_hash         (RisaDenseString* string);
void               risa_dense_string_hash_inplace (RisaDenseString* string);
RisaDenseString*   risa_dense_string_from         (const char* chars, uint32_t length);
RisaDenseString*   risa_dense_string_concat       (RisaDenseString* left, RisaDenseString* right);
void               risa_dense_string_delete       (RisaDenseString* string);

RisaDenseArray*    risa_dense_array_create        ();
void               risa_dense_array_init          (RisaDenseArray* array);
void               risa_dense_array_delete        (RisaDenseArray* array);
RisaValue          risa_dense_array_get           (RisaDenseArray* array, uint32_t index);
void               risa_dense_array_set           (RisaDenseArray* array, uint32_t index, RisaValue value);

RisaDenseObject*   risa_dense_object_create       ();
RisaDenseObject*   risa_dense_object_create_under (void* vm, uint32_t entryCount, ...);
void               risa_dense_object_init         (RisaDenseObject* object);
void               risa_dense_object_delete       (RisaDenseObject* object);
bool               risa_dense_object_get          (RisaDenseObject* object, RisaDenseString* key, RisaValue* value);
void               risa_dense_object_set          (RisaDenseObject* object, RisaDenseString* key, RisaValue value);

RisaDenseUpvalue*  risa_dense_upvalue_create      (RisaValue* value);

RisaDenseFunction* risa_dense_function_create     ();
void               risa_dense_function_init       (RisaDenseFunction* function);
void               risa_dense_function_delete     (RisaDenseFunction* function);

RisaDenseClosure*  risa_dense_closure_create      (RisaDenseFunction* function, uint8_t upvalueCount);

RisaDenseNative*   risa_dense_native_create       (RisaNativeFunction function);
RisaValue          risa_dense_native_value        (RisaNativeFunction function);

#endif
