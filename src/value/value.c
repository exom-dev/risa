#include "value.h"

#include "../common/logging.h"

void value_print(Value value) {
    switch(value.type) {
        case VAL_NULL:
            PRINT("null");
            break;
        case VAL_BOOL:
            PRINT(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_INT:
            PRINT("%lld", AS_INT(value));
            break;
        case VAL_FLOAT:
            PRINT("%f", AS_FLOAT(value));
            break;
    }
}
