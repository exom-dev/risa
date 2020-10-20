#include "value.h"
#include "dense.h"
#include "../common/logging.h"

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
        case VAL_DENSE:
            dense_print(AS_DENSE(value));
            break;
    }
}

Value value_clone(Value value) {
    switch(value.type) {
        case VAL_NULL:
        case VAL_BOOL:
        case VAL_BYTE:
        case VAL_INT:
        case VAL_FLOAT:
            return value;
        case VAL_DENSE:
            return dense_clone(AS_DENSE(value));
    }
}

bool value_is_falsy(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool value_equals(Value left, Value right) {
    if(left.type != right.type) {
        if(IS_BYTE(left)) {
            if(IS_INT(right))
                return AS_BYTE(left) == AS_INT(right);
            if(IS_FLOAT(right))
                return AS_BYTE(left) == AS_FLOAT(right);
            return false;
        } else if(IS_INT(left)) {
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
        case VAL_DENSE: return AS_DENSE(left) == AS_DENSE(right);
        default: return false;
    }
}

bool value_strict_equals(Value left, Value right) {
    if(left.type != right.type)
        return false;
    if(left.type == VAL_DENSE && AS_DENSE(left)->type != AS_DENSE(right)->type)
        return false;

    switch(left.type) {
        case VAL_NULL:  return true;
        case VAL_BOOL:  return AS_BOOL(left) == AS_BOOL(right);
        case VAL_BYTE:  return AS_BYTE(left) == AS_BYTE(right);
        case VAL_INT:   return AS_INT(left) == AS_INT(right);
        case VAL_FLOAT: return AS_FLOAT(left) == AS_FLOAT(right);
        case VAL_DENSE: return AS_DENSE(left) == AS_DENSE(right);
        default: return false;
    }
}

bool value_is_dense_of_type(Value value, DenseValueType type) {
    return IS_DENSE(value) && AS_DENSE(value)->type == type;
}