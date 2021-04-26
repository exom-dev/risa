#include "dense.h"
#include "../vm/vm.h"

DenseObject* dense_object_create() {
    DenseObject* object = (DenseObject*) RISA_MEM_ALLOC(sizeof(DenseObject));

    dense_object_init(object);
    return object;
}

DenseObject* dense_object_create_register(void* vm, uint32_t entryCount, ...) {
    DenseObject* obj = dense_object_create();

    va_list args;

    va_start(args, entryCount);

    for(uint32_t i = 0; i < entryCount; ++i) {
        char* key        = va_arg(args, char*);
        uint32_t keySize = va_arg(args, uint32_t);
        Value val        = va_arg(args, Value);

        dense_object_set(obj, vm_string_internalize((VM*) vm, key, keySize), val);

        if(val.type == VAL_DENSE)
            vm_register_dense((VM*) vm, AS_DENSE(val));
    }

    va_end(args);

    vm_register_dense((VM*) vm, ((DenseValue*) obj));

    return obj;
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
