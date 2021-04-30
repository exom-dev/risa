#include "value.h"
#include "dense.h"
#include "../io/log.h"
#include "../def/def.h"
#include "../def/macro.h"

#include <string.h>

void value_print(RisaIO* io, Value value) {
    switch(value.type) {
        case VAL_NULL:
            RISA_OUT((*io), "null");
            break;
        case VAL_BOOL:
            RISA_OUT((*io), AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_BYTE:
            RISA_OUT((*io), "%hhu", AS_BYTE(value));
            break;
        case VAL_INT:
            RISA_OUT((*io), "%lld", AS_INT(value));
            break;
        case VAL_FLOAT:
            RISA_OUT((*io), "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            break;
        case VAL_DENSE:
            dense_print(io, AS_DENSE(value));
            break;
        default:
            RISA_OUT((*io), "UNK");
            break;
    }
}

char* value_to_string(Value value) {
    char* data;

    switch(value.type) {
        case VAL_NULL: {
            data = RISA_MEM_ALLOC(sizeof("null"));
            memcpy(data, "null\0", sizeof("null"));
            break;
        }
        case VAL_BOOL: {
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
        case VAL_BYTE: {
            size_t size = 1 + snprintf(NULL, 0, "%hhu", AS_BYTE(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%hhu", AS_BYTE(value));
            break;
        }
        case VAL_INT: {
            size_t size = 1 + snprintf(NULL, 0, "%lld", AS_INT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%lld", AS_INT(value));
            break;
        }
        case VAL_FLOAT: {
            size_t size = 1 + snprintf(NULL, 0, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", AS_FLOAT(value));
            break;
        }
        case VAL_DENSE: {
            data = dense_to_string(AS_DENSE(value));
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
        default:
            return NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

Value value_clone_register(void* vm, Value value) {
    switch(value.type) {
        case VAL_NULL:
        case VAL_BOOL:
        case VAL_BYTE:
        case VAL_INT:
        case VAL_FLOAT:
            return value;
        case VAL_DENSE:
            return dense_clone_register(vm, AS_DENSE(value));
        default:
            return NULL_VALUE;  // Never reached; written to suppress warnings.
    }
}

bool value_is_truthy(Value value) {
    switch(value.type) {
        case VAL_NULL:
            return false;
        case VAL_BOOL:
            return AS_BOOL(value);
        case VAL_BYTE:
            return AS_BYTE(value) != 0;
        case VAL_INT:
            return AS_INT(value) != 0;
        case VAL_FLOAT:
            return AS_FLOAT(value) != 0;
        case VAL_DENSE:
            return dense_is_truthy(AS_DENSE(value));
        default:
            return false;  // Never reached; written to suppress warnings.
    }
}

bool value_is_falsy(Value value) {
    return !value_is_truthy(value);
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