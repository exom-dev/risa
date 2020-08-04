#include "dense.h"

DenseArray* dense_array_create() {
    DenseArray* array = (DenseArray*) MEM_ALLOC(sizeof(DenseArray));

    dense_array_init(array);
    return array;
}

void dense_array_init(DenseArray* array) {
    array->dense.type = DVAL_ARRAY;
    array->dense.link = NULL;
    array->dense.marked = false;

    value_array_init(&array->data);
}

void dense_array_delete(DenseArray* array) {
    dense_array_init(array);
}

Value dense_array_get(DenseArray* array, uint32_t index) {
    return array->data.values[index];
}

void dense_array_set(DenseArray* array, uint32_t index, Value value) {
    if(index == array->data.size)
        value_array_write(&array->data, value);
    else array->data.values[index] = value;
}