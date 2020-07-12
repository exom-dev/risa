#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

#include "../common/headers.h"

typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT
} ValueType;

typedef struct {
    ValueType type;

    union {
        bool boolean;
        int64_t integer;
        double floating;
    } as;
} Value;

#define NULL_VALUE         ((Value) { VAL_NULL, { .integer = 0 } })
#define BOOL_VALUE(value)  ((Value) { VAL_BOOL, { .boolean = value} })
#define INT_VALUE(value)   ((Value) { VAL_INT, { .integer = value } })
#define FLOAT_VALUE(value) ((Value) { VAL_FLOAT, { .floating = value } })

#define AS_BOOL(value)  ((value).as.boolean)
#define AS_INT(value)   ((value).as.integer)
#define AS_FLOAT(value) ((value).as.floating)

#define IS_NULL(value)  ((value).type == VAL_NULL)
#define IS_BOOL(value)  ((value).type == VAL_BOOL)
#define IS_INT(value)   ((value).type == VAL_INT)
#define IS_FLOAT(value) ((value).type == VAL_FLOAT)

typedef struct {
    size_t size;
    size_t capacity;

    Value* values;
} ValueArray;

void value_print(Value value);

ValueArray* value_array_create();
void value_array_init(ValueArray* array);
void value_array_write(ValueArray* array, Value value);
void value_array_delete(ValueArray* array);

#endif
