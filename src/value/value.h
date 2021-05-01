#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

#include "../api.h"
#include "../io/io.h"
#include "../def/types.h"

typedef enum {
    RISA_VAL_NULL,
    RISA_VAL_BOOL,
    RISA_VAL_BYTE,
    RISA_VAL_INT,
    RISA_VAL_FLOAT,
    RISA_VAL_DENSE
} RisaValueType;

typedef enum {
    RISA_DVAL_STRING,
    RISA_DVAL_ARRAY,
    RISA_DVAL_OBJECT,
    RISA_DVAL_UPVALUE,
    RISA_DVAL_FUNCTION,
    RISA_DVAL_CLOSURE,
    RISA_DVAL_NATIVE
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
    size_t size;
    size_t capacity;

    RisaValue* values;
} RisaValueArray;

#define RISA_NULL_VALUE         ((RisaValue) { RISA_VAL_NULL, { .integer = 0 } })
#define RISA_BOOL_VALUE(value)  ((RisaValue) { RISA_VAL_BOOL, { .boolean = value} })
#define RISA_BYTE_VALUE(value)  ((RisaValue) { RISA_VAL_BYTE, { .byte = value } })
#define RISA_INT_VALUE(value)   ((RisaValue) { RISA_VAL_INT, { .integer = value } })
#define RISA_FLOAT_VALUE(value) ((RisaValue) { RISA_VAL_FLOAT, { .floating = value } })
#define RISA_DENSE_VALUE(value) ((RisaValue) { RISA_VAL_DENSE, { .dense = (RisaDenseValue*) value } })

#define RISA_AS_BOOL(value)     ((value).as.boolean)
#define RISA_AS_BYTE(value)     ((value).as.byte)
#define RISA_AS_INT(value)      ((value).as.integer)
#define RISA_AS_FLOAT(value)    ((value).as.floating)
#define RISA_AS_DENSE(value)    ((value).as.dense)

#define RISA_IS_NULL(value)     ((value).type == RISA_VAL_NULL)
#define RISA_IS_BOOL(value)     ((value).type == RISA_VAL_BOOL)
#define RISA_IS_BYTE(value)     ((value).type == RISA_VAL_BYTE)
#define RISA_IS_INT(value)      ((value).type == RISA_VAL_INT)
#define RISA_IS_FLOAT(value)    ((value).type == RISA_VAL_FLOAT)
#define RISA_IS_DENSE(value)    ((value).type == RISA_VAL_DENSE)

RISA_API void      risa_value_print             (RisaIO* io, RisaValue value);
RISA_API char*     risa_value_to_string         (RisaValue value);
RISA_API RisaValue risa_value_clone             (RisaValue value);
RISA_API RisaValue risa_value_clone_register    (void* vm, RisaValue value); // void* in order to work around the circular dependency.
RISA_API bool      risa_value_is_truthy         (RisaValue value);
RISA_API bool      risa_value_is_falsy          (RisaValue value);
RISA_API bool      risa_value_equals            (RisaValue left, RisaValue right);
RISA_API bool      risa_value_strict_equals     (RisaValue left, RisaValue right);

RISA_API bool      risa_value_is_dense_of_type  (RisaValue value, RisaDenseValueType type);

// Length is required for the first 2 in order to determine the base (prefixes 0x, 0b, ..., may or may not exist)
RISA_API RisaValue risa_value_int_from_string   (char* str, uint32_t length);
RISA_API RisaValue risa_value_byte_from_string  (char* str, uint32_t length);
RISA_API RisaValue risa_value_float_from_string (char* str);
RISA_API RisaValue risa_value_bool_from_string  (char* str);

RISA_API void      risa_value_array_init        (RisaValueArray* array);
RISA_API void      risa_value_array_write       (RisaValueArray* array, RisaValue value);
RISA_API void      risa_value_array_delete      (RisaValueArray* array);

#endif
