#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

#include "../api.h"
#include "../io/io.h"
#include "../def/types.h"

typedef enum {
    RISA_VAL_NULL  = 0,
    RISA_VAL_BOOL  = 1,
    RISA_VAL_BYTE  = 2,
    RISA_VAL_INT   = 3,
    RISA_VAL_FLOAT = 4,
    RISA_VAL_DENSE = 5
} RisaValueType;

typedef enum {
    RISA_DVAL_STRING   = 0,
    RISA_DVAL_ARRAY    = 1,
    RISA_DVAL_OBJECT   = 2,
    RISA_DVAL_UPVALUE  = 3,
    RISA_DVAL_FUNCTION = 4,
    RISA_DVAL_CLOSURE  = 5,
    RISA_DVAL_NATIVE   = 6
} RisaDenseValueType;

typedef struct RisaDenseValue {
    RisaDenseValueType type;
    struct RisaDenseValue* link;
    bool marked;
} RisaDenseValue;

typedef struct {
    RisaValueType type;

    union {
        bool boolean;
        uint8_t byte;
        int64_t integer;
        double floating;
        RisaDenseValue* dense;
    } as;
} RisaValue;

typedef struct {
    uint32_t size;
    uint32_t capacity;

    RisaValue* values;
} RisaValueArray;

RISA_API void      risa_value_print             (RisaIO* io, RisaValue value);
RISA_API char*     risa_value_to_string         (RisaValue value);
RISA_API RisaValue risa_value_clone             (RisaValue value);
RISA_API RisaValue risa_value_clone_register    (void* vm, RisaValue value); // void* in order to work around the circular dependency.
RISA_API bool      risa_value_is_truthy         (RisaValue value);
RISA_API bool      risa_value_is_falsy          (RisaValue value);
RISA_API bool      risa_value_equals            (RisaValue left, RisaValue right);
RISA_API bool      risa_value_strict_equals     (RisaValue left, RisaValue right);

RISA_API bool      risa_value_is_dense_of_type  (RisaValue value, RisaDenseValueType type);

RISA_API RisaValue risa_value_from_null         ();
RISA_API RisaValue risa_value_from_bool         (bool value);
RISA_API RisaValue risa_value_from_byte         (uint8_t value);
RISA_API RisaValue risa_value_from_int          (uint64_t value);
RISA_API RisaValue risa_value_from_float        (double value);
RISA_API RisaValue risa_value_from_dense        (RisaDenseValue* value);

RISA_API bool            risa_value_as_bool     (RisaValue value);
RISA_API uint8_t         risa_value_as_byte     (RisaValue value);
RISA_API int64_t         risa_value_as_int      (RisaValue value);
RISA_API double          risa_value_as_float    (RisaValue value);
RISA_API RisaDenseValue* risa_value_as_dense    (RisaValue value);

RISA_API bool            risa_value_is_null     (RisaValue value);
RISA_API bool            risa_value_is_bool     (RisaValue value);
RISA_API bool            risa_value_is_byte     (RisaValue value);
RISA_API bool            risa_value_is_int      (RisaValue value);
RISA_API bool            risa_value_is_float    (RisaValue value);
RISA_API bool            value_is_dense         (RisaValue value);

RISA_API bool            risa_value_is_num      (RisaValue value);

// Length is required for the first 2 in order to determine the base (prefixes 0x, 0b, ..., may or may not exist)
RISA_API RisaValue risa_value_int_from_string   (char* str, uint32_t length);
RISA_API RisaValue risa_value_byte_from_string  (char* str, uint32_t length);
RISA_API RisaValue risa_value_float_from_string (char* str);
RISA_API RisaValue risa_value_bool_from_string  (char* str);

RISA_API void      risa_value_array_init        (RisaValueArray* array);
RISA_API void      risa_value_array_write       (RisaValueArray* array, RisaValue value);
RISA_API void      risa_value_array_delete      (RisaValueArray* array);

#endif
