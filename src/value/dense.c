#include "dense.h"
#include "../common/logging.h"

void dense_print(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            PRINT("%s", ((DenseString*) dense)->chars);
            break;
        case DVAL_ARRAY:
            PRINT("[");
            for(uint32_t i = 0; i < ((DenseArray*) dense)->data.size; ++i) {
                value_print(((DenseArray*) dense)->data.values[i]);
                if(i < ((DenseArray*) dense)->data.size - 1)
                    PRINT(", ");
            }
            PRINT("]");
            break;
        case DVAL_OBJECT: {
            bool first = true;

            PRINT("{ ");
            for (uint32_t i = 0; i < ((DenseObject *) dense)->data.capacity; ++i) {
                if(((DenseObject *) dense)->data.entries[i].key != NULL) {
                    if(first)
                        first = false;
                    else PRINT(", ");

                    PRINT("\"");
                    dense_print((DenseValue *) (((DenseObject *) dense)->data.entries[i].key));
                    PRINT("\": ");

                    value_print(((DenseObject *) dense)->data.entries[i].value);
                }
            }
            PRINT(" }");
            break;
        }
        case DVAL_UPVALUE:
            PRINT("<upval>");
            break;
        case DVAL_FUNCTION:
            if(((DenseFunction*) dense)->name == NULL)
                PRINT("<script>");
            else PRINT("<fn %s>", ((DenseFunction*) dense)->name->chars);
            break;
        case DVAL_CLOSURE:
            if(((DenseClosure*) dense)->function->name == NULL)
                PRINT("<script>");
            else PRINT("<fn %s>", ((DenseClosure*) dense)->function->name->chars);
            break;
        case DVAL_NATIVE:
            PRINT("<native fn>");
            break;
        default:
            PRINT("UNK");
    }
}

Value  dense_clone(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return DENSE_VALUE(dense);
        case DVAL_ARRAY: {
            DenseArray* array = (DenseArray*) dense;
            DenseArray* clone = dense_array_create();

            for(size_t i = 0; i < array->data.size; ++i)
                value_array_write(&clone->data, array->data.values[i]);

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
    }
}

void dense_delete(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            MEM_FREE(dense);
            break;
        case DVAL_ARRAY:
            dense_array_delete((DenseArray*) dense);
            MEM_FREE(dense);
            break;
        case DVAL_OBJECT:
            dense_object_delete((DenseObject*) dense);
            MEM_FREE(dense);
            break;
        case DVAL_UPVALUE:
        case DVAL_NATIVE:
            MEM_FREE(dense);
            break;
        case DVAL_FUNCTION:
            chunk_delete(&((DenseFunction*) dense)->chunk);
            MEM_FREE(dense);
            break;
        case DVAL_CLOSURE:
            MEM_FREE(((DenseClosure *) dense)->upvalues);
            MEM_FREE(dense);
            break;
    }
}
