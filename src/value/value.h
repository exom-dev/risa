#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

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

#define NULL_VALUE         ((RisaValue) { RISA_VAL_NULL, { .integer = 0 } })
#define BOOL_VALUE(value)  ((RisaValue) { RISA_VAL_BOOL, { .boolean = value} })
#define BYTE_VALUE(value)  ((RisaValue) { RISA_VAL_BYTE, { .byte = value } })
#define INT_VALUE(value)   ((RisaValue) { RISA_VAL_INT, { .integer = value } })
#define FLOAT_VALUE(value) ((RisaValue) { RISA_VAL_FLOAT, { .floating = value } })
#define DENSE_VALUE(value) ((RisaValue) { RISA_VAL_DENSE, { .dense = (RisaDenseValue*) value } })

#define AS_BOOL(value)     ((value).as.boolean)
#define AS_BYTE(value)     ((value).as.byte)
#define AS_INT(value)      ((value).as.integer)
#define AS_FLOAT(value)    ((value).as.floating)
#define AS_DENSE(value)    ((value).as.dense)

#define IS_NULL(value)     ((value).type == RISA_VAL_NULL)
#define IS_BOOL(value)     ((value).type == RISA_VAL_BOOL)
#define IS_BYTE(value)     ((value).type == RISA_VAL_BYTE)
#define IS_INT(value)      ((value).type == RISA_VAL_INT)
#define IS_FLOAT(value)    ((value).type == RISA_VAL_FLOAT)
#define IS_DENSE(value)    ((value).type == RISA_VAL_DENSE)

void      value_print             (RisaIO* io, RisaValue value);
char*     value_to_string         (RisaValue value);
RisaValue value_clone             (RisaValue value);
RisaValue value_clone_register    (void* vm, RisaValue value); // void* in order to work around the circular dependency.
bool      value_is_truthy         (RisaValue value);
bool      value_is_falsy          (RisaValue value);
bool      value_equals            (RisaValue left, RisaValue right);
bool      value_strict_equals     (RisaValue left, RisaValue right);

bool      value_is_dense_of_type  (RisaValue value, RisaDenseValueType type);

// Length is required for the first 2 in order to determine the base (prefixes 0x, 0b, ..., may or may not exist)
RisaValue value_int_from_string   (char* str, uint32_t length);
RisaValue value_byte_from_string  (char* str, uint32_t length);
RisaValue value_float_from_string (char* str);
RisaValue value_bool_from_string  (char* str);

void      value_array_init        (RisaValueArray* array);
void      value_array_write       (RisaValueArray* array, RisaValue value);
void      value_array_delete      (RisaValueArray* array);

#endif
