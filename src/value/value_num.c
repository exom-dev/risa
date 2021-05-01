#include "value.h"
#include "../lib/charlib.h"

#include <stdlib.h>

static RisaValue value_num_from_string(char* str, uint32_t length) {
    if(str == NULL)
        return NULL_VALUE;

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

    if (!risa_lib_charlib_strtol(str + offset, base, &num)) {
        return NULL_VALUE;
    }

    return INT_VALUE(num);
}

RisaValue value_int_from_string(char* str, uint32_t length) {
    return value_num_from_string(str, length);
}

RisaValue value_byte_from_string(char* str, uint32_t length) {
    RisaValue result = value_num_from_string(str, length);

    return result.type == RISA_VAL_NULL ? result : BYTE_VALUE((uint8_t) AS_INT(result));
}

RisaValue value_float_from_string(char* str) {
    double num;

    if(!risa_lib_charlib_strtod(str, &num)) {
        return NULL_VALUE;
    }

    return FLOAT_VALUE(num);
}

RisaValue value_bool_from_string(char* str) {
    if(risa_lib_charlib_stricmp(str, "true") || risa_lib_charlib_stricmp(str, "1"))
        return BOOL_VALUE(true);
    if(risa_lib_charlib_stricmp(str, "false") || risa_lib_charlib_stricmp(str, "0"))
        return BOOL_VALUE(false);

    return NULL_VALUE;
}