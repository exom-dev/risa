#include "dense.h"
#include "../common/logging.h"

void dense_print(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            PRINT("%s", ((DenseString*) dense)->chars);
            break;
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

size_t dense_size(DenseValue* dense) {
    switch(dense->type) {
        case DVAL_STRING:
            return sizeof(DenseString) + ((DenseString*) dense)->length + 1;
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
