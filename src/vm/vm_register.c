#include "vm.h"

void vm_register_string(VM* vm, DenseString* string) {
    map_set(&vm->strings, string, NULL_VALUE);
    vm_register_dense(vm, (DenseValue*) string);
}

void vm_register_dense(VM* vm, DenseValue* dense) {
    if(dense == NULL)
        return;

    DenseValue* it = vm->values;
    bool exists = false;

    while(it != NULL) {
        if(it == dense) {
            exists = true;
            break;
        }

        it = it->link;
    }

    if(exists)
        return;

    dense->link = vm->values;
    vm->values = dense;

    vm->heapSize += dense_size(dense);

    if(dense->type == DVAL_FUNCTION) {
        vm_register_dense(vm, (DenseValue*) ((DenseFunction*) dense)->name);

        ValueArray* constants = &((DenseFunction*) dense)->chunk.constants;

        for(size_t i = 0; i < constants->size; ++i)
            if (IS_DENSE(constants->values[i]))
                vm_register_dense(vm, AS_DENSE(constants->values[i]));
    } else if(dense->type == DVAL_CLOSURE) {
        vm_register_dense(vm, (DenseValue*) ((DenseClosure*) dense)->function->name);

        ValueArray* constants = &((DenseClosure*) dense)->function->chunk.constants;

        for(size_t i = 0; i < constants->size; ++i)
            if (IS_DENSE(constants->values[i]))
                vm_register_dense(vm, AS_DENSE(constants->values[i]));
    }
}
