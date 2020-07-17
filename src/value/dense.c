#include "dense.h"
#include "../common/logging.h"

void dense_print(DenseValue* value) {
    switch(value->type) {
        case DVAL_STRING:
            PRINT("%s", ((DenseString*) value)->chars);
            break;
        case DVAL_FUNCTION:
            if(((DenseFunction*) value)->name == NULL)
                PRINT("<script>");
            else PRINT("<fn %s>", ((DenseFunction*) value)->name->chars);
            break;
        case DVAL_NATIVE:
            PRINT("<native fn>");
    }
}
