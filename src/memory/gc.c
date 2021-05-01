#include "gc.h"
#include "../vm/vm.h"
#include "../io/log.h"

#include <string.h>

static void gc_mark_dense(RisaDenseValue* dense);
static void gc_mark_map(RisaMap* map);
static void gc_sweep(RisaVM* vm);

void risa_gc_check(RisaVM* vm) {
    if(vm->heapSize >= vm->heapThreshold) {
        risa_gc_run(vm);
        vm->heapThreshold *= 2;
    }
}

void risa_gc_run(RisaVM* vm) {
    for(RisaValue* i = vm->stack; i < vm->stackTop; ++i)
        if(IS_DENSE(*i))
            gc_mark_dense(AS_DENSE(*i));

    for(uint32_t i = 0; i < vm->frameCount; ++i)
        gc_mark_dense((RisaDenseValue*) VM_FRAME_FUNCTION(vm->frames[i]));

    for(RisaDenseUpvalue* i = vm->upvalues; i != NULL; i = (RisaDenseUpvalue*) i->next)
        gc_mark_dense((RisaDenseValue*) i);

    gc_mark_map(&vm->globals);

    for(uint32_t i = 0; i < vm->strings.capacity; ++i) {
        RisaMapEntry* entry = &vm->strings.entries[i];

        if(entry->key != NULL && !((RisaDenseString*) entry->key)->dense.marked)
            risa_map_erase(&vm->strings, entry->key);
    }

    gc_sweep(vm);
}

static void gc_mark_dense(RisaDenseValue* dense) {
    if(dense == NULL || dense->marked)
        return;
    dense->marked = true;

    switch(dense->type) {
        case RISA_DVAL_STRING:
        case RISA_DVAL_NATIVE:
            break;
        case RISA_DVAL_ARRAY: {
            RisaDenseArray* array = (RisaDenseArray*) dense;

            for(uint32_t i = 0; i < array->data.size; ++i)
                if(IS_DENSE(array->data.values[i]))
                    gc_mark_dense(AS_DENSE(array->data.values[i]));
            break;
        }
        case RISA_DVAL_OBJECT:
            gc_mark_map(&((RisaDenseObject*) dense)->data);
            break;
        case RISA_DVAL_UPVALUE:
            if(IS_DENSE(((RisaDenseUpvalue*) dense)->closed))
                gc_mark_dense(AS_DENSE(((RisaDenseUpvalue*) dense)->closed));
            break;
        case RISA_DVAL_FUNCTION: {
            RisaDenseFunction* function = (RisaDenseFunction*) dense;
            gc_mark_dense((RisaDenseValue*) function->name);

            for(uint32_t i = 0; i < function->cluster.constants.size; ++i)
                if(IS_DENSE(function->cluster.constants.values[i]))
                    gc_mark_dense(AS_DENSE(function->cluster.constants.values[i]));
            break;
        }
        case RISA_DVAL_CLOSURE: {
            RisaDenseClosure* closure = (RisaDenseClosure*) dense;
            gc_mark_dense((RisaDenseValue*) closure->function);

            for(uint32_t i = 0; i < closure->upvalueCount; ++i)
                gc_mark_dense((RisaDenseValue*) closure->upvalues[i]);
            break;
        }
    }
}

static void gc_mark_map(RisaMap* map) {
    for(uint32_t i = 0; i < map->capacity; ++i) {
        RisaMapEntry* entry = &map->entries[i];
        gc_mark_dense((RisaDenseValue*) entry->key);

        if(IS_DENSE(entry->value))
            gc_mark_dense(AS_DENSE(entry->value));
    }
}

static void gc_sweep(RisaVM* vm) {
    RisaDenseValue* prev = NULL;
    RisaDenseValue* dense = vm->values;

    while(dense != NULL) {
        if(dense->marked) {
            dense->marked = false;
            prev = dense;
            dense = dense->link;
        } else {
            RisaDenseValue* unmarked = dense;

            dense = dense->link;

            if(prev != NULL)
                prev->link = dense;
            else vm->values = dense;

            vm->heapSize -= risa_dense_size(unmarked);

            risa_dense_delete(unmarked);
        }
    }
}