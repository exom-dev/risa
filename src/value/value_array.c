#include "value.h"

#include "../memory/mem.h"

void value_array_init(ValueArray* array) {
    array->size = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_write(ValueArray* array, Value value) {
    while(array->capacity <= array->size)
        array->values = (Value*) RISA_MEM_EXPAND(array->values, &array->capacity, sizeof(Value));

    array->values[array->size++] = value;
}

void value_array_delete(ValueArray* array) {
    RISA_MEM_FREE(array->values);
    value_array_init(array);
}
