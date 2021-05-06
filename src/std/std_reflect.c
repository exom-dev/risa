#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

static RisaValue risa_std_reflect_reflect (void*, uint8_t, RisaValue*);

void risa_std_register_reflect(RisaVM* vm) {
    #define STD_REFLECT_ENTRY(name) RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_std_reflect_##name

    risa_vm_global_set_native(vm, STD_REFLECT_ENTRY(reflect));

    #undef STD_REFLECT_ENTRY
}

static RisaValue risa_std_reflect_reflect(void* vm, uint8_t argc, RisaValue* args) {
    switch(argc) {
        case 0: {
            RisaDenseObject* obj = risa_dense_object_create_under(vm, 0);

            risa_map_copy(&obj->data, &((RisaVM*) vm)->globals);

            return RISA_DENSE_VALUE(obj);
        }
        case 1: {
            if(!risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING))
                return RISA_NULL_VALUE;

            RisaValue val;

            if(!risa_map_get(&((RisaVM*) vm)->globals, RISA_AS_STRING(args[0]), &val))
                return RISA_NULL_VALUE;

            return val;
        }
        default: {
            if(!risa_value_is_dense_of_type(args[0], RISA_DVAL_STRING))
                return RISA_NULL_VALUE;

            risa_map_set(&((RisaVM*) vm)->globals, RISA_AS_STRING(args[0]), args[1]);

            return args[1];
        }
    }
}