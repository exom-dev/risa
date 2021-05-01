#include "dense.h"

RisaDenseArray* risa_dense_array_create() {
    RisaDenseArray* array = (RisaDenseArray*) RISA_MEM_ALLOC(sizeof(RisaDenseArray));

    risa_dense_array_init(array);
    return array;
}

void risa_dense_array_init(RisaDenseArray* array) {
    array->dense.type = RISA_DVAL_ARRAY;
    array->dense.link = NULL;
    array->dense.marked = false;

    risa_value_array_init(&array->data);
}

void risa_dense_array_delete(RisaDenseArray* array) {
    risa_value_array_delete(&array->data);
    risa_dense_array_init(array);
}

RisaValue risa_dense_array_get(RisaDenseArray* array, uint32_t index) {
    return array->data.values[index];
}

void risa_dense_array_set(RisaDenseArray* array, uint32_t index, RisaValue value) {
    if(index == array->data.size)
        risa_value_array_write(&array->data, value);
    else array->data.values[index] = value;
}