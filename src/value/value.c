#include "value.h"
#include "dense.h"
#include "../io/log.h"
#include "../def/def.h"
#include "../def/macro.h"

#include <string.h>

void value_print(RisaIO* io, RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
            RISA_OUT((*io), "null");
            break;
        case RISA_VAL_BOOL:
            RISA_OUT((*io), AS_BOOL(value) ? "true" : "false");
            break;
        case RISA_VAL_BYTE:
            RISA_OUT((*io), "%hhu", AS_BYTE(value));
            break;
        case RISA_VAL_INT:
            RISA_OUT((*io), "%lld", AS_INT(value));
            break;
        case RISA_VAL_FLOAT:
            RISA_OUT((*io), "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            break;
        case RISA_VAL_DENSE:
            risa_dense_print(io, AS_DENSE(value));
            break;
        default:
            RISA_OUT((*io), "UNK");
            break;
    }
}

char* value_to_string(RisaValue value) {
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

            if(AS_BOOL(value)) {
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
            size_t size = 1 + snprintf(NULL, 0, "%hhu", AS_BYTE(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%hhu", AS_BYTE(value));
            break;
        }
        case RISA_VAL_INT: {
            size_t size = 1 + snprintf(NULL, 0, "%lld", AS_INT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%lld", AS_INT(value));
            break;
        }
        case RISA_VAL_FLOAT: {
            size_t size = 1 + snprintf(NULL, 0, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            break;
        }
        case RISA_VAL_DENSE: {
            data = risa_dense_to_string(AS_DENSE(value));
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

RisaValue value_clone(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
        case RISA_VAL_BOOL:
        case RISA_VAL_BYTE:
        case RISA_VAL_INT:
        case RISA_VAL_FLOAT:
            return value;
        case RISA_VAL_DENSE:
            return risa_dense_clone(AS_DENSE(value));
        default:
            return NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

RisaValue value_clone_register(void* vm, RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
        case RISA_VAL_BOOL:
        case RISA_VAL_BYTE:
        case RISA_VAL_INT:
        case RISA_VAL_FLOAT:
            return value;
        case RISA_VAL_DENSE:
            return risa_dense_clone_under(vm, AS_DENSE(value));
        default:
            return NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

bool value_is_truthy(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
            return false;
        case RISA_VAL_BOOL:
            return AS_BOOL(value);
        case RISA_VAL_BYTE:
            return AS_BYTE(value) != 0;
        case RISA_VAL_INT:
            return AS_INT(value) != 0;
        case RISA_VAL_FLOAT:
            return AS_FLOAT(value) != 0;
        case RISA_VAL_DENSE:
            return risa_dense_is_truthy(AS_DENSE(value));
        default:
            return false;  // Never reached; written to suppress warnings.
    }
}

bool value_is_falsy(RisaValue value) {
    return !value_is_truthy(value);
}

bool value_equals(RisaValue left, RisaValue right) {
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
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return AS_BOOL(left) == AS_BOOL(right);
        case RISA_VAL_BYTE:  return AS_BYTE(left) == AS_BYTE(right);
        case RISA_VAL_INT:   return AS_INT(left) == AS_INT(right);
        case RISA_VAL_FLOAT: return AS_FLOAT(left) == AS_FLOAT(right);
        case RISA_VAL_DENSE: return AS_DENSE(left) == AS_DENSE(right);
        default: return false;
    }
}

bool value_strict_equals(RisaValue left, RisaValue right) {
    if(left.type != right.type)
        return false;
    if(left.type == RISA_VAL_DENSE && AS_DENSE(left)->type != AS_DENSE(right)->type)
        return false;

    switch(left.type) {
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return AS_BOOL(left) == AS_BOOL(right);
        case RISA_VAL_BYTE:  return AS_BYTE(left) == AS_BYTE(right);
        case RISA_VAL_INT:   return AS_INT(left) == AS_INT(right);
        case RISA_VAL_FLOAT: return AS_FLOAT(left) == AS_FLOAT(right);
        case RISA_VAL_DENSE: return AS_DENSE(left) == AS_DENSE(right);
        default: return false;
    }
}

bool value_is_dense_of_type(RisaValue value, RisaDenseValueType type) {
    return IS_DENSE(value) && AS_DENSE(value)->type == type;
}