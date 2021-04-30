#include "value.h"
#include "../lib/charlib.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static Value value_num_from_string(char* str, uint32_t length) {
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

Value value_int_from_string(char* str, uint32_t length) {
    return value_num_from_string(str, length);
}

Value value_byte_from_string(char* str, uint32_t length) {
    Value result = value_num_from_string(str, length);

    return result.type == VAL_NULL ?  result : BYTE_VALUE((uint8_t) AS_INT(result));
}

Value value_float_from_string(char* str) {
    double num;

    if(!risa_lib_charlib_strtod(str, &num)) {
        return NULL_VALUE;
    }

    return FLOAT_VALUE(num);
}