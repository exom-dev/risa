#include "dense.h"
#include "../io/log.h"
#include "../vm/vm.h"
#include "../lib/charlib.h"

#include <string.h>

void risa_dense_print(RisaIO* io, RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            RISA_OUT((*io), "%s", ((RisaDenseString*) dense)->chars);
            break;
        case RISA_DVAL_ARRAY:
            RISA_OUT((*io), "[");
            for(uint32_t i = 0; i < ((RisaDenseArray*) dense)->data.size; ++i) {
                risa_value_print(io, ((RisaDenseArray *) dense)->data.values[i]);
                if(i < ((RisaDenseArray*) dense)->data.size - 1)
                    RISA_OUT((*io), ", ");
            }
            RISA_OUT((*io), "]");
            break;
        case RISA_DVAL_OBJECT: {
            bool first = true;

            RISA_OUT((*io), "{ ");
            for (uint32_t i = 0; i < ((RisaDenseObject *) dense)->data.capacity; ++i) {
                if(((RisaDenseObject *) dense)->data.entries[i].key != NULL) {
                    if(first)
                        first = false;
                    else RISA_OUT((*io), ", ");

                    RISA_OUT((*io), "\"");
                    risa_dense_print(io, (RisaDenseValue *) (((RisaDenseObject *) dense)->data.entries[i].key));
                    RISA_OUT((*io), "\": ");

                    risa_value_print(io, ((RisaDenseObject *) dense)->data.entries[i].value);
                }
            }
            RISA_OUT((*io), " }");
            break;
        }
        case RISA_DVAL_UPVALUE:
            RISA_OUT((*io), "<upval>");
            break;
        case RISA_DVAL_FUNCTION:
            if(((RisaDenseFunction*) dense)->name == NULL)
                RISA_OUT((*io), "<script>");
            else RISA_OUT((*io), "<fn %s>", ((RisaDenseFunction*) dense)->name->chars);
            break;
        case RISA_DVAL_CLOSURE:
            if(((RisaDenseClosure*) dense)->function->name == NULL)
                RISA_OUT((*io), "<script>");
            else RISA_OUT((*io), "<fn %s>", ((RisaDenseClosure*) dense)->function->name->chars);
            break;
        case RISA_DVAL_NATIVE:
            RISA_OUT((*io), "<native fn>");
            break;
        default:
            RISA_OUT((*io), "UNK");
            break;
    }
}

char* risa_dense_to_string(RisaDenseValue* dense) {
    char* data;

    switch(dense->type) {
        case RISA_DVAL_STRING: {
            data = RISA_MEM_ALLOC(sizeof(char) * (1 + ((RisaDenseString*) dense)->length));
            memcpy(data, ((RisaDenseString*) dense)->chars, ((RisaDenseString*) dense)->length + 1);
            break;
        }
        case RISA_DVAL_ARRAY: {
            RisaLibCharlibString str;

            risa_lib_charlib_string_init(&str);
            risa_lib_charlib_string_append_c(&str,  "[");

            for(uint32_t i = 0; i < ((RisaDenseArray*) dense)->data.size; ++i) {
                char* valStr = risa_value_to_string(((RisaDenseArray *) dense)->data.values[i]);

                risa_lib_charlib_string_append_c(&str, valStr);
                RISA_MEM_FREE(valStr);

                if(i < ((RisaDenseArray*) dense)->data.size - 1)
                    risa_lib_charlib_string_append_c(&str,  ", ");
            }

            risa_lib_charlib_string_append_c(&str,  "]");

            data = str.data;
            break;
        }
        case RISA_DVAL_OBJECT: {
            bool first = true;

            RisaLibCharlibString str;

            risa_lib_charlib_string_init(&str);
            risa_lib_charlib_string_append_c(&str,  "{ ");

            for (uint32_t i = 0; i < ((RisaDenseObject *) dense)->data.capacity; ++i) {
                if(((RisaDenseObject *) dense)->data.entries[i].key != NULL) {
                    if(first)
                        first = false;
                    else risa_lib_charlib_string_append_c(&str,  ", ");

                    risa_lib_charlib_string_append_c(&str,  "\"");

                    char* valStr = risa_dense_to_string((RisaDenseValue *) (((RisaDenseObject *) dense)->data.entries[i].key));
                    risa_lib_charlib_string_append_c(&str,  valStr);
                    RISA_MEM_FREE(valStr);


                    risa_lib_charlib_string_append_c(&str,  "\": ");

                    valStr = risa_value_to_string(((RisaDenseObject *) dense)->data.entries[i].value);
                    risa_lib_charlib_string_append_c(&str,  valStr);
                    RISA_MEM_FREE(valStr);
                }
            }

            risa_lib_charlib_string_append_c(&str,  " }");

            data = str.data;
            break;
        }
        case RISA_DVAL_UPVALUE: {
            data = RISA_MEM_ALLOC(sizeof("<upval>"));
            memcpy(data, "<upval>\0", sizeof("<upval>"));
            break;
        }
        case RISA_DVAL_FUNCTION: {
            if(((RisaDenseFunction*) dense)->name == NULL) {
                data = RISA_MEM_ALLOC(sizeof("<script>"));
                memcpy(data, "<script>\0", sizeof("<script>"));
                break;
            } else {
                RisaLibCharlibString str;

                risa_lib_charlib_string_init(&str);
                risa_lib_charlib_string_append_c(&str, "<fn ");
                risa_lib_charlib_string_append_c(&str, ((RisaDenseFunction*) dense)->name->chars);
                risa_lib_charlib_string_append_c(&str, ">");

                data = str.data;
                break;
            }
        }
        case RISA_DVAL_CLOSURE: {
            if(((RisaDenseFunction*) dense)->name == NULL) {
                data = RISA_MEM_ALLOC(sizeof("<script>"));
                memcpy(data, "<script>\0", sizeof("<script>"));
                break;
            } else {
                RisaLibCharlibString str;

                risa_lib_charlib_string_init(&str);
                risa_lib_charlib_string_append_c(&str, "<fn ");
                risa_lib_charlib_string_append_c(&str, ((RisaDenseClosure*) dense)->function->name->chars);
                risa_lib_charlib_string_append_c(&str, ">");

                data = str.data;
                break;
            }
        }
        case RISA_DVAL_NATIVE: {
            data = RISA_MEM_ALLOC(sizeof("<native fn>"));
            memcpy(data, "<native fn>\0", sizeof("<native fn>"));
            break;
        }
        default: {
            data = RISA_MEM_ALLOC(sizeof("UNK"));
            memcpy(data, "UNK\0", sizeof("UNK"));
            break;
        }
    }

    return data;
}

bool risa_dense_is_truthy(RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            return ((RisaDenseString*) dense)->length > 0;
        case RISA_DVAL_ARRAY:
            return ((RisaDenseArray*) dense)->data.size > 0;
        case RISA_DVAL_OBJECT:
            return ((RisaDenseObject*) dense)->data.count > 0;
        case RISA_DVAL_UPVALUE:
        case RISA_DVAL_FUNCTION:
        case RISA_DVAL_CLOSURE:
        case RISA_DVAL_NATIVE:
            return true;
        default:
            return false; // Never reached; written to suppress warnings
    }
}

RisaValue risa_dense_clone(RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            return risa_value_from_dense(dense);
        case RISA_DVAL_ARRAY: {
            RisaDenseArray* array = (RisaDenseArray*) dense;
            RisaDenseArray* clone = risa_dense_array_create();

            for(size_t i = 0; i < array->data.size; ++i)
                risa_value_array_write(&clone->data, risa_value_clone(array->data.values[i]));

            return risa_value_from_dense(((RisaDenseValue*) clone));
        }
        case RISA_DVAL_OBJECT: {
            RisaDenseObject* object = (RisaDenseObject*) dense;
            RisaDenseObject* clone = risa_dense_object_create();

            for(size_t i = 0; i < object->data.capacity; ++i) {
                RisaMapEntry entry = object->data.entries[i];

                if(entry.key != NULL)
                    risa_dense_object_set(clone, entry.key, risa_value_clone(entry.value));
            }

            return risa_value_from_dense(((RisaDenseValue*) clone));
        }
        case RISA_DVAL_UPVALUE: {
            RisaDenseUpvalue* upvalue = (RisaDenseUpvalue*) dense;
            RisaDenseUpvalue* clone = risa_dense_upvalue_create(upvalue->ref);

            if(upvalue->closed.type != RISA_VAL_NULL)
                clone->closed = risa_value_clone(upvalue->closed);

            return risa_value_from_dense(((RisaDenseValue*) clone));
        }
        case RISA_DVAL_FUNCTION:
        case RISA_DVAL_CLOSURE:
        case RISA_DVAL_NATIVE:
            return risa_value_from_dense(dense);
        default:
            return risa_value_from_null(); // Never reached; written to suppress warnings.
    }
}

RisaValue risa_dense_clone_under(void* vm, RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            return risa_value_from_dense(dense);
        case RISA_DVAL_ARRAY: {
            RisaDenseArray* array = (RisaDenseArray*) dense;
            RisaDenseArray* clone = risa_dense_array_create();

            for(size_t i = 0; i < array->data.size; ++i) {
                risa_value_array_write(&clone->data, risa_value_clone_register(vm, array->data.values[i]));
            }

            RisaDenseValue* result = ((RisaDenseValue*) clone);
            risa_vm_register_dense_unchecked((RisaVM *) vm, result);

            return risa_value_from_dense(result);
        }
        case RISA_DVAL_OBJECT: {
            RisaDenseObject* object = (RisaDenseObject*) dense;
            RisaDenseObject* clone = risa_dense_object_create();

            for(size_t i = 0; i < object->data.capacity; ++i) {
                RisaMapEntry entry = object->data.entries[i];

                if(entry.key != NULL) {
                    risa_dense_object_set(clone, entry.key, risa_value_clone_register(vm, entry.value));
                }
            }

            RisaDenseValue* result = ((RisaDenseValue*) clone);
            risa_vm_register_dense_unchecked((RisaVM *) vm, result);

            return risa_value_from_dense(result);
        }
        case RISA_DVAL_UPVALUE: {
            RisaDenseUpvalue* upvalue = (RisaDenseUpvalue*) dense;
            RisaDenseUpvalue* clone = risa_dense_upvalue_create(upvalue->ref);

            if(upvalue->closed.type != RISA_VAL_NULL) {
                clone->closed = risa_value_clone(upvalue->closed);
            }

            RisaDenseValue* result = ((RisaDenseValue*) clone);
            risa_vm_register_dense_unchecked((RisaVM *) vm, result);

            return risa_value_from_dense(result);
        }
        case RISA_DVAL_FUNCTION:
        case RISA_DVAL_CLOSURE:
        case RISA_DVAL_NATIVE:
            return risa_value_from_dense(dense);
        default:
            return risa_value_from_null(); // Never reached; written to suppress warnings.
    }
}

size_t risa_dense_size(RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            return sizeof(RisaDenseString) + ((RisaDenseString*) dense)->length + 1;
        case RISA_DVAL_ARRAY:
            return sizeof(RisaDenseArray);
        case RISA_DVAL_OBJECT:
            return sizeof(RisaDenseObject);
        case RISA_DVAL_UPVALUE:
            return sizeof(RisaDenseUpvalue);
        case RISA_DVAL_FUNCTION:
            return sizeof(RisaDenseFunction) + ((RisaDenseFunction*) dense)->cluster.capacity * (sizeof(uint8_t) + sizeof(uint32_t))
                   + ((RisaDenseFunction*) dense)->cluster.constants.capacity * sizeof(RisaValue);
        case RISA_DVAL_NATIVE:
            return sizeof(RisaDenseNative);
        case RISA_DVAL_CLOSURE:
            return ((RisaDenseClosure*) dense)->upvalueCount * sizeof(RisaDenseUpvalue) + sizeof(RisaDenseClosure);
        default:
            return 0;  // Never reached; written to suppress warnings.
    }
}

RisaDenseValueType risa_dense_get_type(RisaDenseValue* dense) {
    return dense->type;
}

void risa_dense_delete(RisaDenseValue* dense) {
    switch(dense->type) {
        case RISA_DVAL_STRING:
            RISA_MEM_FREE(dense);
            break;
        case RISA_DVAL_ARRAY:
            risa_dense_array_delete((RisaDenseArray*) dense);
            RISA_MEM_FREE(dense);
            break;
        case RISA_DVAL_OBJECT:
            risa_dense_object_delete((RisaDenseObject*) dense);
            RISA_MEM_FREE(dense);
            break;
        case RISA_DVAL_UPVALUE:
        case RISA_DVAL_NATIVE:
            RISA_MEM_FREE(dense);
            break;
        case RISA_DVAL_FUNCTION:
            risa_dense_function_delete((RisaDenseFunction*) dense);
            RISA_MEM_FREE(dense);
            break;
        case RISA_DVAL_CLOSURE:
            RISA_MEM_FREE(((RisaDenseClosure *) dense)->upvalues);
            RISA_MEM_FREE(dense);
            break;
    }
}