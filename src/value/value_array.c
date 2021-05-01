#include "value.h"

#include "../mem/mem.h"

void value_array_init(RisaValueArray* array) {
    array->size = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_write(RisaValueArray* array, RisaValue value) {
    while(array->capacity <= array->size)
        array->values = (RisaValue*) RISA_MEM_EXPAND(array->values, &array->capacity, sizeof(RisaValue));

    array->values[array->size++] = value;
}

void value_array_delete(RisaValueArray* array) {
    RISA_MEM_FREE(array->values);
    value_array_init(array);
}
