#include "vm.h"

void risa_vm_register_string(RisaVM* vm, RisaDenseString* string) {
    risa_map_set(&vm->strings, string, RISA_NULL_VALUE);
    risa_vm_register_dense(vm, (RisaDenseValue *) string);
}

void risa_vm_register_dense(RisaVM* vm, RisaDenseValue* dense) {
    if(dense == NULL)
        return;

    RisaDenseValue* it = vm->values;

    while(it != NULL) {
        if(it == dense)
            return;

        it = it->link;
    }

    risa_vm_register_dense_unchecked(vm, dense);
}

void risa_vm_register_dense_unchecked(RisaVM* vm, RisaDenseValue* dense) {
    dense->link = vm->values;
    vm->values = dense;

    vm->heapSize += risa_dense_size(dense);

    switch(dense->type) {
        case RISA_DVAL_FUNCTION: {
            risa_vm_register_dense(vm, (RisaDenseValue *) ((RisaDenseFunction *) dense)->name);

            RisaValueArray *constants = &((RisaDenseFunction *) dense)->cluster.constants;

            for(size_t i = 0; i < constants->size; ++i)
                if(RISA_IS_DENSE(constants->values[i]))
                    risa_vm_register_dense(vm, RISA_AS_DENSE(constants->values[i]));

            break;
        }
        case RISA_DVAL_CLOSURE: {
            risa_vm_register_dense(vm, (RisaDenseValue *) ((RisaDenseClosure *) dense)->function->name);

            RisaValueArray *constants = &((RisaDenseClosure *) dense)->function->cluster.constants;

            for(size_t i = 0; i < constants->size; ++i)
                if(RISA_IS_DENSE(constants->values[i]))
                    risa_vm_register_dense(vm, RISA_AS_DENSE(constants->values[i]));

            break;
        }
        default:
            return;
    }
}
