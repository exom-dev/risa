#include "value.h"

#include "../memory/mem.h"

#include <stdio.h>

void value_print(Value value) {
    printf("%g", value);
}

ValueArray* value_array_create() {
    ValueArray* array = mem_alloc(sizeof(ValueArray));
    value_array_init(array);

    return array;
}

void value_array_init(ValueArray* array) {
    array->size = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_write(ValueArray* array, Value value) {
    if(array->capacity <= array->size)
        array->values = mem_expand(array->values, &array->capacity);

    array->values[array->size++] = value;
}

void value_array_delete(ValueArray* array) {
    mem_free(array->values);

    value_array_init(array);
}
