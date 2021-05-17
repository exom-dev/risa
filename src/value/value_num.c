#include "value.h"
#include "../lib/charlib.h"

#include <stdlib.h>

static RisaValue value_num_from_string(char* str, uint32_t length) {
    if(str == NULL)
        return RISA_NULL_VALUE;

    // 0xF -> skip the first 2 chars, parse in base 16.
    // 230 -> don't skip any chars, parse in base 10.
    size_t offset = 0;
    uint32_t base = 10;

    if(length > 2 && str[0] == '0') {
        offset = 2;

        switch(str[1]) {
            case 'x':
                base = 16;
                break;
            case 'b':
                base = 2;
                break;
            default:
                offset = 0;
                break;
        }
    }

    int64_t num;

    if (!risa_lib_charlib_strtoll(str + offset, base, &num)) {
        return RISA_NULL_VALUE;
    }

    return RISA_INT_VALUE(num);
}

bool risa_value_is_num(RisaValue value) {
    switch(value.type) {
        case RISA_VAL_BYTE:
        case RISA_VAL_INT:
        case RISA_VAL_FLOAT:
            return true;
        default:
            return false;
    }
}

RisaValue risa_value_int_from_string(char* str, uint32_t length) {
    return value_num_from_string(str, length);
}

RisaValue risa_value_byte_from_string(char* str, uint32_t length) {
    RisaValue result = value_num_from_string(str, length);

    return result.type == RISA_VAL_NULL ? result : RISA_BYTE_VALUE((uint8_t) RISA_AS_INT(result));
}

RisaValue risa_value_float_from_string(char* str) {
    double num;

    if(!risa_lib_charlib_strtod(str, &num)) {
        return RISA_NULL_VALUE;
    }

    return RISA_FLOAT_VALUE(num);
}

RisaValue risa_value_bool_from_string(char* str) {
    if(risa_lib_charlib_stricmp(str, "true") || risa_lib_charlib_stricmp(str, "1"))
        return RISA_BOOL_VALUE(true);
    if(risa_lib_charlib_stricmp(str, "false") || risa_lib_charlib_stricmp(str, "0"))
        return RISA_BOOL_VALUE(false);

    return RISA_NULL_VALUE;
}