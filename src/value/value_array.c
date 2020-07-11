#include "value.h"

#include "../memory/mem.h"
#include "../common/logging.h"

ValueArray* value_array_create() {
    ValueArray* array = (ValueArray*) MEM_ALLOC(sizeof(ValueArray));
    value_array_init(array);

    return array;
}

void value_array_init(ValueArray* array) {
    array->size = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_write(ValueArray* array, Value value) {
    while(array->capacity <= array->size * sizeof(Value))
        array->values = (Value*) MEM_EXPAND(array->values, &array->capacity);

    array->values[array->size++] = value;
}

void value_array_delete(ValueArray* array) {
    MEM_FREE(array->values);
    value_array_init(array);
}
