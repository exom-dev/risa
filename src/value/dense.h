#ifndef RISA_DENSE_H
#define RISA_DENSE_H

#include "../api.h"
#include "../io/io.h"
#include "../def/types.h"
#include "../cluster/cluster.h"
#include "../mem/mem.h"
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

#define RISA_AS_STRING(value)   ((RisaDenseString*) ((value).as.dense))
#define RISA_AS_ARRAY(value)    ((RisaDenseArray*) ((value).as.dense))
#define RISA_AS_OBJECT(value)   ((RisaDenseObject*) ((value).as.dense))
#define RISA_AS_CSTRING(value)  (((RisaDenseString*) ((value).as.dense))->chars)
#define RISA_AS_FUNCTION(value) ((RisaDenseFunction*) ((value).as.dense))
#define RISA_AS_CLOSURE(value)  ((RisaDenseClosure*) ((value).as.dense))
#define RISA_AS_NATIVE(value)   ((RisaDenseNative*) ((value).as.dense))

RISA_API void               risa_dense_print               (RisaIO* io, RisaDenseValue* dense);
RISA_API char*              risa_dense_to_string           (RisaDenseValue* dense);
RISA_API bool               risa_dense_is_truthy           (RisaDenseValue* dense);
RISA_API RisaValue          risa_dense_clone               (RisaDenseValue* dense);
RISA_API RisaValue          risa_dense_clone_under         (void* vm, RisaDenseValue* dense); // void* instead of RisaVM* in order to work around the circular dependency.
RISA_API size_t             risa_dense_size                (RisaDenseValue* dense);
RISA_API RisaDenseValueType risa_dense_get_type            (RisaDenseValue* dense);
RISA_API void               risa_dense_delete              (RisaDenseValue* dense);

RISA_API RisaDenseString*   risa_dense_string_prepare      (const char* chars, uint32_t length);
RISA_API uint32_t           risa_dense_string_hash         (RisaDenseString* string);
RISA_API void               risa_dense_string_hash_inplace (RisaDenseString* string);
RISA_API RisaDenseString*   risa_dense_string_from         (const char* chars, uint32_t length);
RISA_API RisaDenseString*   risa_dense_string_concat       (RisaDenseString* left, RisaDenseString* right);
RISA_API void               risa_dense_string_delete       (RisaDenseString* string);

RISA_API RisaDenseArray*    risa_dense_array_create        ();
RISA_API void               risa_dense_array_init          (RisaDenseArray* array);
RISA_API void               risa_dense_array_delete        (RisaDenseArray* array);
RISA_API RisaValue          risa_dense_array_get           (RisaDenseArray* array, uint32_t index);
RISA_API void               risa_dense_array_set           (RisaDenseArray* array, uint32_t index, RisaValue value);

RISA_API RisaDenseObject*   risa_dense_object_create       ();
RISA_API RisaDenseObject*   risa_dense_object_create_under (void* vm, uint32_t entryCount, ...);
RISA_API void               risa_dense_object_init         (RisaDenseObject* object);
RISA_API void               risa_dense_object_delete       (RisaDenseObject* object);
RISA_API bool               risa_dense_object_get          (RisaDenseObject* object, RisaDenseString* key, RisaValue* value);
RISA_API void               risa_dense_object_set          (RisaDenseObject* object, RisaDenseString* key, RisaValue value);

RISA_API RisaDenseUpvalue*  risa_dense_upvalue_create      (RisaValue* value);

RISA_API RisaDenseFunction* risa_dense_function_create      ();
RISA_API void               risa_dense_function_init        (RisaDenseFunction* function);
RISA_API RisaCluster*       risa_dense_function_get_cluster (RisaDenseFunction* function);
RISA_API void               risa_dense_function_delete      (RisaDenseFunction* function);
RISA_API void               risa_dense_function_free        (RisaDenseFunction* function);

RISA_API RisaDenseClosure*  risa_dense_closure_create      (RisaDenseFunction* function, uint8_t upvalueCount);

RISA_API RisaDenseNative*   risa_dense_native_create       (RisaNativeFunction function);
RISA_API RisaValue          risa_dense_native_value        (RisaNativeFunction function);
RISA_API RisaValue          risa_dense_native_get_arg      (RisaValue* args, uint8_t index);

#endif
