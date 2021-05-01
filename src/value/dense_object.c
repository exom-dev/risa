#include "dense.h"
#include "../vm/vm.h"

RisaDenseObject* risa_dense_object_create() {
    RisaDenseObject* object = (RisaDenseObject*) RISA_MEM_ALLOC(sizeof(RisaDenseObject));

    risa_dense_object_init(object);
    return object;
}

RisaDenseObject* risa_dense_object_create_under(void* vm, uint32_t entryCount, ...) {
    RisaDenseObject* obj = risa_dense_object_create();

    va_list args;

    va_start(args, entryCount);

    for(uint32_t i = 0; i < entryCount; ++i) {
        char* key        = va_arg(args, char*);
        uint32_t keySize = va_arg(args, uint32_t);
        RisaValue val        = va_arg(args, RisaValue);

        risa_dense_object_set(obj, risa_vm_string_create((RisaVM *) vm, key, keySize), val);

        if(val.type == RISA_VAL_DENSE)
            risa_vm_register_dense((RisaVM *) vm, RISA_AS_DENSE(val));
    }

    va_end(args);

    risa_vm_register_dense((RisaVM *) vm, ((RisaDenseValue *) obj));

    return obj;
}

void risa_dense_object_init(RisaDenseObject* object) {
    object->dense.type = RISA_DVAL_OBJECT;
    object->dense.link = NULL;
    object->dense.marked = false;

    risa_map_init(&object->data);
}

void risa_dense_object_delete(RisaDenseObject* object) {
    risa_map_delete(&object->data);
    risa_dense_object_init(object);
}

bool risa_dense_object_get(RisaDenseObject* object, RisaDenseString* key, RisaValue* value) {
    return risa_map_get(&object->data, key, value);
}

void risa_dense_object_set(RisaDenseObject* object, RisaDenseString* key, RisaValue value) {
    risa_map_set(&object->data, key, value);
}
