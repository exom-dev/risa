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
            RISA_OUT((*io), risa_value_as_bool(value) ? "true" : "false");
            break;
        case RISA_VAL_BYTE:
            RISA_OUT((*io), "%hhu", risa_value_as_byte(value));
            break;
        case RISA_VAL_INT:
            RISA_OUT((*io), "%lld", risa_value_as_int(value));
            break;
        case RISA_VAL_FLOAT:
            RISA_OUT((*io), "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", risa_value_as_float(value));
            break;
        case RISA_VAL_DENSE:
            risa_dense_print(io, risa_value_as_dense(value));
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

            if(risa_value_as_bool(value)) {
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
            size_t size = 1 + snprintf(NULL, 0, "%hhu", risa_value_as_byte(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%hhu", risa_value_as_byte(value));
            break;
        }
        case RISA_VAL_INT: {
            size_t size = 1 + snprintf(NULL, 0, "%lld", risa_value_as_int(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%lld", risa_value_as_int(value));
            break;
        }
        case RISA_VAL_FLOAT: {
            size_t size = 1 + snprintf(NULL, 0, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", risa_value_as_float(value));
            data = RISA_MEM_ALLOC(sizeof(char) * size);

            snprintf(data, size, "%." RISA_STRINGIFY(RISA_VALUE_FLOAT_PRECISION) "g", risa_value_as_float(value));
            break;
        }
        case RISA_VAL_DENSE: {
            data = risa_dense_to_string(risa_value_as_dense(value));
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
            return risa_dense_clone(risa_value_as_dense(value));
        default:
            return risa_value_from_null();  // Never reached; written to suppress warnings.
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
            return risa_dense_clone_under(vm, risa_value_as_dense(value));
        default:
            return risa_value_from_null();  // Never reached; written to suppress warnings.
    }
}

bool risa_value_is_truthy(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_NULL:
            return false;
        case RISA_VAL_BOOL:
            return risa_value_as_bool(value);
        case RISA_VAL_BYTE:
            return risa_value_as_byte(value) != 0;
        case RISA_VAL_INT:
            return risa_value_as_int(value) != 0;
        case RISA_VAL_FLOAT:
            return risa_value_as_float(value) != 0;
        case RISA_VAL_DENSE:
            return risa_dense_is_truthy(risa_value_as_dense(value));
        default:
            return false;  // Never reached; written to suppress warnings.
    }
}

bool risa_value_is_falsy(RisaValue value) {
    return !risa_value_is_truthy(value);
}

bool risa_value_equals(RisaValue left, RisaValue right) {
    if(left.type != right.type) {
        if(risa_value_is_byte(left)) {
            if(risa_value_is_int(right))
                return risa_value_as_byte(left) == risa_value_as_int(right);
            if(risa_value_is_float(right))
                return risa_value_as_byte(left) == risa_value_as_float(right);
            return false;
        } else if(risa_value_is_int(left)) {
            if(risa_value_is_byte(right))
                return risa_value_as_int(left) == risa_value_as_byte(right);
            if(risa_value_is_float(right))
                return risa_value_as_int(left) == risa_value_as_float(right);
            return false;
        } else if(risa_value_is_float(left)) {
            if(risa_value_is_byte(right))
                return risa_value_as_float(left) == risa_value_as_byte(right);
            if(risa_value_is_int(right))
                return risa_value_as_float(left) == risa_value_as_int(right);
            return false;
        } else return false;
    }

    switch(left.type) {
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return risa_value_as_bool(left) == risa_value_as_bool(right);
        case RISA_VAL_BYTE:  return risa_value_as_byte(left) == risa_value_as_byte(right);
        case RISA_VAL_INT:   return risa_value_as_int(left) == risa_value_as_int(right);
        case RISA_VAL_FLOAT: return risa_value_as_float(left) == risa_value_as_float(right);
        case RISA_VAL_DENSE: return risa_value_as_dense(left) == risa_value_as_dense(right);
        default: return false;
    }
}

bool risa_value_strict_equals(RisaValue left, RisaValue right) {
    if(left.type != right.type)
        return false;
    if(left.type == RISA_VAL_DENSE && risa_value_as_dense(left)->type != risa_value_as_dense(right)->type)
        return false;

    switch(left.type) {
        case RISA_VAL_NULL:  return true;
        case RISA_VAL_BOOL:  return risa_value_as_bool(left) == risa_value_as_bool(right);
        case RISA_VAL_BYTE:  return risa_value_as_byte(left) == risa_value_as_byte(right);
        case RISA_VAL_INT:   return risa_value_as_int(left) == risa_value_as_int(right);
        case RISA_VAL_FLOAT: return risa_value_as_float(left) == risa_value_as_float(right);
        case RISA_VAL_DENSE: return risa_value_as_dense(left) == risa_value_as_dense(right);
        default: return false;
    }
}

bool risa_value_is_dense_of_type(RisaValue value, RisaDenseValueType type) {
    return value_is_dense(value) && risa_value_as_dense(value)->type == type;
}

RisaValue risa_value_from_null() {
    RisaValue value;
    value.type = RISA_VAL_NULL;
    value.as.integer = 0;

    return value;
}

RisaValue risa_value_from_bool(bool value) {
    RisaValue v;
    v.type = RISA_VAL_BOOL;
    v.as.boolean = value;

    return v;
}

RisaValue risa_value_from_byte(uint8_t value) {
    RisaValue v;
    v.type = RISA_VAL_BYTE;
    v.as.byte = value;

    return v;
}

RisaValue risa_value_from_int(uint64_t value) {
    RisaValue v;
    v.type = RISA_VAL_INT;
    v.as.integer = value;

    return v;
}

RisaValue risa_value_from_float(double value) {
    RisaValue v;
    v.type = RISA_VAL_FLOAT;
    v.as.floating = value;

    return v;
}

RisaValue risa_value_from_dense(RisaDenseValue* value) {
    RisaValue v;
    v.type = RISA_VAL_DENSE;
    v.as.dense = value;

    return v;
}

bool risa_value_as_bool(RisaValue value) {
    if(value.type == RISA_VAL_BOOL) {
        return value.as.boolean;
    }

    return false;
}

uint8_t risa_value_as_byte(RisaValue value) {
    switch(value.type) {
    case RISA_VAL_BYTE:
        return value.as.byte;
    case RISA_VAL_INT:
        return (uint8_t) value.as.integer;
    case RISA_VAL_FLOAT:
        return (uint8_t) value.as.floating;
    default:
        return 0;
    }
}

int64_t risa_value_as_int(RisaValue value) {
    switch(value.type) {
    case RISA_VAL_BYTE:
        return (int64_t) value.as.byte;
    case RISA_VAL_INT:
        return value.as.integer;
    case RISA_VAL_FLOAT:
        return (int64_t) value.as.floating;
    default:
        return 0;
    }
}

double risa_value_as_float(RisaValue value) {
    switch(value.type) {
    case RISA_VAL_BYTE:
        return (double) value.as.byte;
    case RISA_VAL_INT:
        return (double) value.as.integer;
    case RISA_VAL_FLOAT:
        return value.as.floating;
    default:
        return RISA_VALUE_FLOAT_MIN;
    }
}

RisaDenseValue* risa_value_as_dense(RisaValue value) {
    if(value.type == RISA_VAL_DENSE) {
        return value.as.dense;
    }

    return NULL;
}

bool risa_value_is_null(RisaValue value) {
    return value.type == RISA_VAL_NULL;
}

bool risa_value_is_bool(RisaValue value) {
    return value.type == RISA_VAL_BOOL;
}

bool risa_value_is_byte(RisaValue value) {
    return value.type == RISA_VAL_BYTE;
}

bool risa_value_is_int(RisaValue value) {
    return value.type == RISA_VAL_INT;
}

bool risa_value_is_float(RisaValue value) {
    return value.type == RISA_VAL_FLOAT;
}

bool value_is_dense(RisaValue value) {
    return value.type == RISA_VAL_DENSE;
}