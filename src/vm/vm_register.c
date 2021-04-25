#include "vm.h"

void vm_register_string(VM* vm, DenseString* string) {
    map_set(&vm->strings, string, NULL_VALUE);
    vm_register_dense(vm, (DenseValue*) string);
}

void vm_register_dense(VM* vm, DenseValue* dense) {
    if(dense == NULL)
        return;

    DenseValue* it = vm->values;

    while(it != NULL) {
        if(it == dense)
            return;

        it = it->link;
    }

    vm_register_dense_unchecked(vm, dense);
}

void vm_register_dense_unchecked(VM* vm, DenseValue* dense) {
    dense->link = vm->values;
    vm->values = dense;

    vm->heapSize += dense_size(dense);

    switch(dense->type) {
        case DVAL_FUNCTION: {
            vm_register_dense(vm, (DenseValue *) ((DenseFunction *) dense)->name);

            ValueArray *constants = &((DenseFunction *) dense)->chunk.constants;

            for(size_t i = 0; i < constants->size; ++i)
                if(IS_DENSE(constants->values[i]))
                    vm_register_dense(vm, AS_DENSE(constants->values[i]));

            break;
        }
        case DVAL_CLOSURE: {
            vm_register_dense(vm, (DenseValue *) ((DenseClosure *) dense)->function->name);

            ValueArray *constants = &((DenseClosure *) dense)->function->chunk.constants;

            for(size_t i = 0; i < constants->size; ++i)
                if(IS_DENSE(constants->values[i]))
                    vm_register_dense(vm, AS_DENSE(constants->values[i]));

            break;
        }
        default:
            return;
    }
}
