#include "dense.h"

DenseObject* dense_object_create() {
    DenseObject* object = (DenseObject*) MEM_ALLOC(sizeof(DenseObject));

    dense_object_init(object);
    return object;
}

void dense_object_init(DenseObject* object) {
    object->dense.type = DVAL_OBJECT;
    object->dense.link = NULL;
    object->dense.marked = false;

    map_init(&object->data);
}

void dense_object_delete(DenseObject* object) {
    map_delete(&object->data);
    dense_object_init(object);
}

bool dense_object_get(DenseObject* object, DenseString* key, Value* value) {
    return map_get(&object->data, key, value);
}

void dense_object_set(DenseObject* object, DenseString* key, Value value) {
    map_set(&object->data, key, value);
}
