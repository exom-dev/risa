#include "dense.h"
#include "../io/log.h"
#include "../vm/vm.h"
#include "../lib/charlib.h"

#include <string.h>

void dense_print(RisaIO* io, DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            RISA_OUT((*io), "%s", ((DenseString*) dense)->chars);
            break;
        case DVAL_ARRAY:
            RISA_OUT((*io), "[");
            for(uint32_t i = 0; i < ((DenseArray*) dense)->data.size; ++i) {
                value_print(io, ((DenseArray*) dense)->data.values[i]);
                if(i < ((DenseArray*) dense)->data.size - 1)
                    RISA_OUT((*io), ", ");
            }
            RISA_OUT((*io), "]");
            break;
        case DVAL_OBJECT: {
            bool first = true;

            RISA_OUT((*io), "{ ");
            for (uint32_t i = 0; i < ((DenseObject *) dense)->data.capacity; ++i) {
                if(((DenseObject *) dense)->data.entries[i].key != NULL) {
                    if(first)
                        first = false;
                    else RISA_OUT((*io), ", ");

                    RISA_OUT((*io), "\"");
                    dense_print(io, (DenseValue *) (((DenseObject *) dense)->data.entries[i].key));
                    RISA_OUT((*io), "\": ");

                    value_print(io, ((DenseObject *) dense)->data.entries[i].value);
                }
            }
            RISA_OUT((*io), " }");
            break;
        }
        case DVAL_UPVALUE:
            RISA_OUT((*io), "<upval>");
            break;
        case DVAL_FUNCTION:
            if(((DenseFunction*) dense)->name == NULL)
                RISA_OUT((*io), "<script>");
            else RISA_OUT((*io), "<fn %s>", ((DenseFunction*) dense)->name->chars);
            break;
        case DVAL_CLOSURE:
            if(((DenseClosure*) dense)->function->name == NULL)
                RISA_OUT((*io), "<script>");
            else RISA_OUT((*io), "<fn %s>", ((DenseClosure*) dense)->function->name->chars);
            break;
        case DVAL_NATIVE:
            RISA_OUT((*io), "<native fn>");
            break;
        default:
            RISA_OUT((*io), "UNK");
            break;
    }
}

char*  dense_to_string(DenseValue* dense) {
    char* data;

    switch(dense->type) {
        case DVAL_STRING: {
            data = RISA_MEM_ALLOC(sizeof(char) * (1 + ((DenseString*) dense)->length));
            memcpy(data, ((DenseString*) dense)->chars, ((DenseString*) dense)->length + 1);
            break;
        }
        case DVAL_ARRAY: {
            RisaLibCharlibString str;

            risa_lib_charlib_string_init(&str);
            risa_lib_charlib_string_append_c(&str,  "[");

            for(uint32_t i = 0; i < ((DenseArray*) dense)->data.size; ++i) {
                char* valStr = value_to_string(((DenseArray*) dense)->data.values[i]);

                risa_lib_charlib_string_append_c(&str, valStr);
                RISA_MEM_FREE(valStr);

                if(i < ((DenseArray*) dense)->data.size - 1)
                    risa_lib_charlib_string_append_c(&str,  ", ");
            }

            risa_lib_charlib_string_append_c(&str,  "]");

            data = str.data;
            break;
        }
        case DVAL_OBJECT: {
            bool first = true;

            RisaLibCharlibString str;

            risa_lib_charlib_string_init(&str);
            risa_lib_charlib_string_append_c(&str,  "{ ");

            for (uint32_t i = 0; i < ((DenseObject *) dense)->data.capacity; ++i) {
                if(((DenseObject *) dense)->data.entries[i].key != NULL) {
                    if(first)
                        first = false;
                    else risa_lib_charlib_string_append_c(&str,  ", ");

                    risa_lib_charlib_string_append_c(&str,  "\"");

                    char* valStr = dense_to_string((DenseValue*) (((DenseObject *) dense)->data.entries[i].key));
                    risa_lib_charlib_string_append_c(&str,  valStr);
                    RISA_MEM_FREE(valStr);


                    risa_lib_charlib_string_append_c(&str,  "\": ");

                    valStr = value_to_string(((DenseObject *) dense)->data.entries[i].value);
                    risa_lib_charlib_string_append_c(&str,  valStr);
                    RISA_MEM_FREE(valStr);
                }
            }

            risa_lib_charlib_string_append_c(&str,  " }");

            data = str.data;
            break;
        }
        case DVAL_UPVALUE: {
            data = RISA_MEM_ALLOC(sizeof("<upval>"));
            memcpy(data, "<upval>\0", sizeof("<upval>"));
            break;
        }
        case DVAL_FUNCTION: {
            if(((DenseFunction*) dense)->name == NULL) {
                data = RISA_MEM_ALLOC(sizeof("<script>"));
                memcpy(data, "<script>\0", sizeof("<script>"));
                break;
            } else {
                RisaLibCharlibString str;

                risa_lib_charlib_string_init(&str);
                risa_lib_charlib_string_append_c(&str, "<fn ");
                risa_lib_charlib_string_append_c(&str, ((DenseFunction*) dense)->name->chars);
                risa_lib_charlib_string_append_c(&str, ">");

                data = str.data;
                break;
            }
        }
        case DVAL_CLOSURE: {
            if(((DenseFunction*) dense)->name == NULL) {
                data = RISA_MEM_ALLOC(sizeof("<script>"));
                memcpy(data, "<script>\0", sizeof("<script>"));
                break;
            } else {
                RisaLibCharlibString str;

                risa_lib_charlib_string_init(&str);
                risa_lib_charlib_string_append_c(&str, "<fn ");
                risa_lib_charlib_string_append_c(&str, ((DenseClosure*) dense)->function->name->chars);
                risa_lib_charlib_string_append_c(&str, ">");

                data = str.data;
                break;
            }
        }
        case DVAL_NATIVE: {
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

bool dense_is_truthy(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return ((DenseString*) dense)->length > 0;
        case DVAL_ARRAY:
            return ((DenseArray*) dense)->data.size > 0;
        case DVAL_OBJECT:
            return ((DenseObject*) dense)->data.count > 0;
        case DVAL_UPVALUE:
        case DVAL_FUNCTION:
        case DVAL_CLOSURE:
        case DVAL_NATIVE:
            return true;
    }
}

Value dense_clone(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return DENSE_VALUE(dense);
        case DVAL_ARRAY: {
            DenseArray* array = (DenseArray*) dense;
            DenseArray* clone = dense_array_create();

            for(size_t i = 0; i < array->data.size; ++i)
                value_array_write(&clone->data, value_clone(array->data.values[i]));

            return DENSE_VALUE(((DenseValue*) clone));
        }
        case DVAL_OBJECT: {
            DenseObject* object = (DenseObject*) dense;
            DenseObject* clone = dense_object_create();

            for(size_t i = 0; i < object->data.capacity; ++i) {
                Entry entry = object->data.entries[i];

                if(entry.key != NULL)
                    dense_object_set(clone, entry.key, value_clone(entry.value));
            }

            return DENSE_VALUE(((DenseValue*) clone));
        }
        case DVAL_UPVALUE: {
            DenseUpvalue* upvalue = (DenseUpvalue*) dense;
            DenseUpvalue* clone = dense_upvalue_create(upvalue->ref);

            if(upvalue->closed.type != VAL_NULL)
                clone->closed = value_clone(upvalue->closed);

            return DENSE_VALUE(((DenseValue*) clone));
        }
        case DVAL_FUNCTION:
        case DVAL_CLOSURE:
        case DVAL_NATIVE:
            return DENSE_VALUE(dense);
        default:
            return NULL_VALUE; // Never reached; written to suppress warnings.
    }
}

Value dense_clone_register(void* vm, DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return DENSE_VALUE(dense);
        case DVAL_ARRAY: {
            DenseArray* array = (DenseArray*) dense;
            DenseArray* clone = dense_array_create();

            for(size_t i = 0; i < array->data.size; ++i) {
                value_array_write(&clone->data, value_clone_register(vm, array->data.values[i]));
            }

            DenseValue* result = ((DenseValue*) clone);
            vm_register_dense_unchecked((VM*) vm, result);

            return DENSE_VALUE(result);
        }
        case DVAL_OBJECT: {
            DenseObject* object = (DenseObject*) dense;
            DenseObject* clone = dense_object_create();

            for(size_t i = 0; i < object->data.capacity; ++i) {
                Entry entry = object->data.entries[i];

                if(entry.key != NULL) {
                    dense_object_set(clone, entry.key, value_clone_register(vm, entry.value));
                }
            }

            DenseValue* result = ((DenseValue*) clone);
            vm_register_dense_unchecked((VM*) vm, result);

            return DENSE_VALUE(result);
        }
        case DVAL_UPVALUE: {
            DenseUpvalue* upvalue = (DenseUpvalue*) dense;
            DenseUpvalue* clone = dense_upvalue_create(upvalue->ref);

            if(upvalue->closed.type != VAL_NULL) {
                clone->closed = value_clone(upvalue->closed);
            }

            DenseValue* result = ((DenseValue*) clone);
            vm_register_dense_unchecked((VM*) vm, result);

            return DENSE_VALUE(result);
        }
        case DVAL_FUNCTION:
        case DVAL_CLOSURE:
        case DVAL_NATIVE:
            return DENSE_VALUE(dense);
        default:
            return NULL_VALUE; // Never reached; written to suppress warnings.
    }
}

size_t dense_size(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return sizeof(DenseString) + ((DenseString*) dense)->length + 1;
        case DVAL_ARRAY:
            return sizeof(DenseArray);
        case DVAL_OBJECT:
            return sizeof(DenseObject);
        case DVAL_UPVALUE:
            return sizeof(DenseUpvalue);
        case DVAL_FUNCTION:
            return sizeof(DenseFunction) + ((DenseFunction*) dense)->chunk.capacity * (sizeof(uint8_t) + sizeof(uint32_t))
                   + ((DenseFunction*) dense)->chunk.constants.capacity * sizeof(Value);
        case DVAL_NATIVE:
            return sizeof(DenseNative);
        case DVAL_CLOSURE:
            return ((DenseClosure*) dense)->upvalueCount * sizeof(DenseUpvalue) + sizeof(DenseClosure);
        default:
            return 0;  // Never reached; written to suppress warnings.
    }
}

void dense_delete(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            RISA_MEM_FREE(dense);
            break;
        case DVAL_ARRAY:
            dense_array_delete((DenseArray*) dense);
            RISA_MEM_FREE(dense);
            break;
        case DVAL_OBJECT:
            dense_object_delete((DenseObject*) dense);
            RISA_MEM_FREE(dense);
            break;
        case DVAL_UPVALUE:
        case DVAL_NATIVE:
            RISA_MEM_FREE(dense);
            break;
        case DVAL_FUNCTION:
            chunk_delete(&((DenseFunction*) dense)->chunk);
            RISA_MEM_FREE(dense);
            break;
        case DVAL_CLOSURE:
            RISA_MEM_FREE(((DenseClosure *) dense)->upvalues);
            RISA_MEM_FREE(dense);
            break;
    }
}
