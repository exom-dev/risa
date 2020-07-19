#include "gc.h"
#include "../vm/vm.h"
#include "../common/logging.h"

#include <string.h>

static void gc_mark_dense(DenseValue* dense);
static void gc_mark_map(Map* map);
static void gc_sweep(VM* vm);

void gc_check(VM* vm) {
    if(vm->heapSize >= vm->heapThreshold) {
        gc_run(vm);
        vm->heapThreshold *= 2;
    }
}

void gc_run(VM* vm) {
    for(Value* i = vm->stack; i < vm->stackTop; ++i)
        if(IS_DENSE(*i))
            gc_mark_dense(AS_DENSE(*i));

    for(uint32_t i = 0; i < vm->frameCount; ++i)
        gc_mark_dense((DenseValue*) FRAME_FUNCTION(vm->frames[i]));

    for(DenseUpvalue* i = vm->upvalues; i != NULL; i = i->next)
        gc_mark_dense((DenseValue*) i);

    gc_mark_map(&vm->globals);

    for(uint32_t i = 0; i < vm->strings.capacity; ++i) {
        Entry* entry = &vm->strings.entries[i];

        if(entry->key != NULL && !entry->key->dense.marked)
            map_erase(&vm->strings, entry->key);
    }

    gc_sweep(vm);
}

static void gc_mark_dense(DenseValue* dense) {
    if(dense == NULL || dense->marked)
        return;
    dense->marked = true;

    switch(dense->type) {
        case DVAL_STRING:
        case DVAL_NATIVE:
            break;
        case DVAL_UPVALUE:
            if(IS_DENSE(((DenseUpvalue*) dense)->closed))
                gc_mark_dense(AS_DENSE(((DenseUpvalue*) dense)->closed));
            break;
        case DVAL_FUNCTION: {
            DenseFunction* function = (DenseFunction*) dense;
            gc_mark_dense((DenseValue*) function->name);

            for(uint32_t i = 0; i < function->chunk.constants.size; ++i)
                if(IS_DENSE(function->chunk.constants.values[i]))
                    gc_mark_dense(AS_DENSE(function->chunk.constants.values[i]));
            break;
        }
        case DVAL_CLOSURE: {
            DenseClosure* closure = (DenseClosure*) dense;
            gc_mark_dense((DenseValue*) closure->function);

            for(uint32_t i = 0; i < closure->upvalueCount; ++i)
                gc_mark_dense((DenseValue*) closure->upvalues[i]);
            break;
        }
    }
}

static void gc_mark_map(Map* map) {
    for(uint32_t i = 0; i < map->capacity; ++i) {
        Entry* entry = &map->entries[i];
        gc_mark_dense((DenseValue*) entry->key);

        if(IS_DENSE(entry->value))
            gc_mark_dense(AS_DENSE(entry->value));
    }
}

static void gc_sweep(VM* vm) {
    DenseValue* prev = NULL;
    DenseValue* dense = vm->values;

    while(dense != NULL) {
        if(dense->marked) {
            dense->marked = false;
            prev = dense;
            dense = dense->link;
        } else {
            DenseValue* unmarked = dense;

            dense = dense->link;

            if(prev != NULL)
                prev->link = dense;
            else vm->values = dense;

            vm->heapSize -= dense_size(unmarked);

            dense_delete(unmarked);
        }
    }
}