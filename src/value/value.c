#include "value.h"
#include "dense.h"
#include "../io/log.h"
#include "../def/def.h"
#include "../def/macro.h"

#include <string.h>

void risa_value_print(RisaIO* io, RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
            RISA_OUT((*io), "null");
            break;
        case RISA_VAL_BOOL:
            RISA_OUT((*io), RISA_AS_BOOL(value) ? "true" : "false");
            break;
        case RISA_VAL_BYTE:
            RISA_OUT((*io), "%hhu", RISA_AS_BYTE(value));
            break;
        case RISA_VAL_INT:
            RISA_OUT((*io), "%lld", RISA_AS_INT(value));
            break;
        case RISA_VAL_FLOAT:
            RISA_OUT((*io), "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", RISA_AS_FLOAT(value));
            break;
        case RISA_VAL_DENSE:
            risa_dense_print(io, RISA_AS_DENSE(value));
            break;
        default:
            RISA_OUT((*io), "UNK");
            break;
    }
}

char* risa_value_to_string(RisaValue value) {
    char* data;

    switch(value.type) {
        case RISA_VAL_NULL: {
            data = RISA_MEM_ALLOC(sizeof("null"));
            memcpy(data, "null\0", sizeof("null"));
            break;
        }
        case RISA_VAL_BOOL: {
            char* str;
            size_t size;

            if(RISA_AS_BOOL(value)) {
                str = "true";
                size = sizeof("true");
            } else {
                str = "false";
                size = sizeof("false");
            }

            data = RISA_MEM_ALLOC(sizeof(char) * size);
            memcpy(data, str, size);
            break;
        }
        case RISA_VAL_BYTE: {
            size_t size = 1 + snprintf(NULL, 0, "%hhu", RISA_AS_BYTE(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%hhu", RISA_AS_BYTE(value));
            break;
        }
        case RISA_VAL_INT: {
            size_t size = 1 + snprintf(NULL, 0, "%lld", RISA_AS_INT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%lld", RISA_AS_INT(value));
            break;
        }
        case RISA_VAL_FLOAT: {
            size_t size = 1 + snprintf(NULL, 0, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", RISA_AS_FLOAT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", RISA_AS_FLOAT(value));
            break;
        }
        case RISA_VAL_DENSE: {
            data = risa_dense_to_string(RISA_AS_DENSE(value));
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

RisaValue risa_value_clone(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
        case RISA_VAL_BOOL:
        case RISA_VAL_BYTE:
        case RISA_VAL_INT:
        case RISA_VAL_FLOAT:
            return value;
        case RISA_VAL_DENSE:
            return risa_dense_clone(RISA_AS_DENSE(value));
        default:
            return RISA_NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

RisaValue risa_value_clone_register(void* vm, RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
        case RISA_VAL_BOOL:
        case RISA_VAL_BYTE:
        case RISA_VAL_INT:
        case RISA_VAL_FLOAT:
            return value;
        case RISA_VAL_DENSE:
            return risa_dense_clone_under(vm, RISA_AS_DENSE(value));
        default:
            return RISA_NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

bool risa_value_is_truthy(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
            return false;
        case RISA_VAL_BOOL:
            return RISA_AS_BOOL(value);
        case RISA_VAL_BYTE:
            return RISA_AS_BYTE(value) != 0;
        case RISA_VAL_INT:
            return RISA_AS_INT(value) != 0;
        case RISA_VAL_FLOAT:
            return RISA_AS_FLOAT(value) != 0;
        case RISA_VAL_DENSE:
            return risa_dense_is_truthy(RISA_AS_DENSE(value));
        default:
            return false;  // Never reached; written to suppress warnings.
    }
}

bool risa_value_is_falsy(RisaValue value) {
    return !risa_value_is_truthy(value);
}

bool risa_value_equals(RisaValue left, RisaValue right) {
    if(left.type != right.type) {
        if(RISA_IS_BYTE(left)) {
            if(RISA_IS_INT(right))
                return RISA_AS_BYTE(left) == RISA_AS_INT(right);
            if(RISA_IS_FLOAT(right))
                return RISA_AS_BYTE(left) == RISA_AS_FLOAT(right);
            return false;
        } else if(RISA_IS_INT(left)) {
            if(RISA_IS_BYTE(right))
                return RISA_AS_INT(left) == RISA_AS_BYTE(right);
            if(RISA_IS_FLOAT(right))
                return RISA_AS_INT(left) == RISA_AS_FLOAT(right);
            return false;
        } else if(RISA_IS_FLOAT(left)) {
            if(RISA_IS_BYTE(right))
                return RISA_AS_FLOAT(left) == RISA_AS_BYTE(right);
            if(RISA_IS_INT(right))
                return RISA_AS_FLOAT(left) == RISA_AS_INT(right);
            return false;
        } else return false;
    }

    switch(left.type) {
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return RISA_AS_BOOL(left) == RISA_AS_BOOL(right);
        case RISA_VAL_BYTE:  return RISA_AS_BYTE(left) == RISA_AS_BYTE(right);
        case RISA_VAL_INT:   return RISA_AS_INT(left) == RISA_AS_INT(right);
        case RISA_VAL_FLOAT: return RISA_AS_FLOAT(left) == RISA_AS_FLOAT(right);
        case RISA_VAL_DENSE: return RISA_AS_DENSE(left) == RISA_AS_DENSE(right);
        default: return false;
    }
}

bool risa_value_strict_equals(RisaValue left, RisaValue right) {
    if(left.type != right.type)
        return false;
    if(left.type == RISA_VAL_DENSE && RISA_AS_DENSE(left)->type != RISA_AS_DENSE(right)->type)
        return false;

    switch(left.type) {
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return RISA_AS_BOOL(left) == RISA_AS_BOOL(right);
        case RISA_VAL_BYTE:  return RISA_AS_BYTE(left) == RISA_AS_BYTE(right);
        case RISA_VAL_INT:   return RISA_AS_INT(left) == RISA_AS_INT(right);
        case RISA_VAL_FLOAT: return RISA_AS_FLOAT(left) == RISA_AS_FLOAT(right);
        case RISA_VAL_DENSE: return RISA_AS_DENSE(left) == RISA_AS_DENSE(right);
        default: return false;
    }
}

bool risa_value_is_dense_of_type(RisaValue value, RisaDenseValueType type) {
    return RISA_IS_DENSE(value) && RISA_AS_DENSE(value)->type == type;
}

RisaValue risa_value_from_null() {
    return RISA_NULL_VALUE;
}

RisaValue risa_value_from_bool(bool value) {
    return RISA_BOOL_VALUE(value);
}

RisaValue risa_value_from_byte(uint8_t value) {
    return RISA_BYTE_VALUE(value);
}

RisaValue risa_value_from_int(uint64_t value) {
    return RISA_INT_VALUE(value);
}

RisaValue risa_value_from_float(double value) {
    return RISA_FLOAT_VALUE(value);
}

RisaValue risa_value_from_dense(RisaDenseValue* value) {
    return RISA_DENSE_VALUE(value);
}

double risa_value_as_float(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_BYTE:
            return (double) RISA_AS_BYTE(value);
        case RISA_VAL_INT:
            return (double) RISA_AS_INT(value);
        case RISA_VAL_FLOAT:
            return RISA_AS_FLOAT(value);
        default:
            return RISA_VALUE_FLOAT_MIN;
    }
}