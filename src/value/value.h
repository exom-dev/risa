#ifndef RISA_VALUE_H_GUARD
#define RISA_VALUE_H_GUARD

#include "../common/headers.h"

typedef double Value;

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
