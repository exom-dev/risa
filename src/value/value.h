#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

#include "../common/headers.h"

typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_BYTE,
    VAL_INT,
    VAL_FLOAT,
    VAL_DENSE
} ValueType;

typedef enum {
    DVAL_STRING,
    DVAL_FUNCTION,
} DenseValueType;

typedef struct DenseValue {
    DenseValueType type;
    struct DenseValue* next;
} DenseValue;

typedef struct {
    ValueType type;

    union {
        bool boolean;
        uint8_t byte;
        int64_t integer;
        double floating;
        DenseValue* dense;
    } as;
} Value;

#define NULL_VALUE         ((Value) { VAL_NULL, { .integer = 0 } })
#define BOOL_VALUE(value)  ((Value) { VAL_BOOL, { .boolean = value} })
#define BYTE_VALUE(value)  ((Value) { VAL_BYTE, { .byte = value } })
#define INT_VALUE(value)   ((Value) { VAL_INT, { .integer = value } })
#define FLOAT_VALUE(value) ((Value) { VAL_FLOAT, { .floating = value } })
#define DENSE_VALUE(value) ((Value) { VAL_DENSE, { .dense = (DenseValue*) value } })

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_BYTE(value)    ((value).as.byte)
#define AS_INT(value)     ((value).as.integer)
#define AS_FLOAT(value)   ((value).as.floating)
#define AS_DENSE(value)   ((value).as.dense)

#define IS_NULL(value)  ((value).type == VAL_NULL)
#define IS_BOOL(value)  ((value).type == VAL_BOOL)
#define IS_BYTE(value)  ((value).type == VAL_BYTE)
#define IS_INT(value)   ((value).type == VAL_INT)
#define IS_FLOAT(value) ((value).type == VAL_FLOAT)
#define IS_DENSE(value) ((value).type == VAL_DENSE)

typedef struct {
    size_t size;
    size_t capacity;

    Value* values;
} ValueArray;

void value_print(Value value);
bool value_is_falsy(Value value);
bool value_equals(Value left, Value right);

bool value_is_dense_of_type(Value value, DenseValueType type);

void value_array_init(ValueArray* array);
void value_array_write(ValueArray* array, Value value);
void value_array_delete(ValueArray* array);

#endif
