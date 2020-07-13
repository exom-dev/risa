#include "value.h"

#include "../common/logging.h"

#include <string.h>

void value_print(Value value) {
    switch(value.type) {
        case VAL_NULL:
            PRINT("null");
            break;
        case VAL_BOOL:
            PRINT(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_BYTE:
            PRINT("%hhu", AS_BYTE(value));
            break;
        case VAL_INT:
            PRINT("%lld", AS_INT(value));
            break;
        case VAL_FLOAT:
            PRINT("%f", AS_FLOAT(value));
            break;
        case VAL_LINKED:
            value_print_linked(AS_LINKED(value));
            break;
    }
}

void value_print_linked(LinkedValue* linked) {
    switch(linked->type) {
        case LVAL_STRING:
            PRINT("%s", ((ValString*) linked)->chars);
            break;
    }
}

bool value_is_falsy(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool value_equals(Value left, Value right) {
    if(left.type != right.type) {
        if(IS_BYTE(left)){
            if(IS_INT(right))
                return AS_BYTE(left) == AS_INT(right);
            if(IS_FLOAT(right))
                return AS_BYTE(left) == AS_FLOAT(right);
            return false;
        } else if(IS_INT(left)){
            if(IS_BYTE(right))
                return AS_INT(left) == AS_BYTE(right);
            if(IS_FLOAT(right))
                return AS_INT(left) == AS_FLOAT(right);
            return false;
        } else if(IS_FLOAT(left)) {
            if(IS_BYTE(right))
                return AS_FLOAT(left) == AS_BYTE(right);
            if(IS_INT(right))
                return AS_FLOAT(left) == AS_INT(right);
            return false;
        } else return false;
    }

    switch(left.type) {
        case VAL_NULL:  return true;
        case VAL_BOOL:  return AS_BOOL(left) == AS_BOOL(right);
        case VAL_BYTE:  return AS_BYTE(left) == AS_BYTE(right);
        case VAL_INT:   return AS_INT(left) == AS_INT(right);
        case VAL_FLOAT: return AS_FLOAT(left) == AS_FLOAT(right);
        case VAL_LINKED: {
            return AS_LINKED(left) == AS_LINKED(right);
        }
        default: return false;
    }
}

bool value_is_linked_of_type(Value value, LinkedValueType type) {
    return IS_LINKED(value) && AS_LINKED(value)->type == type;
}