#include "charlib.h"
#include "../memory/mem.h"

#include <string.h>

#define CHARLIB_STRING_CAP_START 8

void risa_lib_charlib_string_init(RisaLibCharlibString* str) {
    str->len = 0;
    str->cap = 0;
    str->data = NULL;
}

void risa_lib_charlib_string_adjust(RisaLibCharlibString* str, size_t size) {
    while(str->len + size > str->cap) {
        if(str->cap == 0)
            str->cap = CHARLIB_STRING_CAP_START;
        else str->cap *= 2;

        str->data = RISA_MEM_REALLOC(str->data, str->cap, sizeof(char));
    }
}

void risa_lib_charlib_string_append(RisaLibCharlibString* str, RisaLibCharlibString* right) {
    risa_lib_charlib_string_adjust(str, right->len);

    str->data[str->len] = '\0';
    strcat(str->data, right->data);

    str->len += right->len;
}

void risa_lib_charlib_string_append_c(RisaLibCharlibString* str, char* right) {
    size_t len = strlen(right);

    risa_lib_charlib_string_adjust(str, len);

    str->data[str->len] = '\0';
    strcat(str->data, right);

    str->len += len;
}

void risa_lib_charlib_string_append_chr(RisaLibCharlibString* str, char right) {
    risa_lib_charlib_string_adjust(str, 1);

    str->data[str->len] = right;
    str->data[str->len + 1] = '\0';

    str->len += 1;
}

RisaLibCharlibString risa_lib_charlib_string_from_sub(const char* src, int start, int end) {
    RisaLibCharlibString str;
    int size = end - start;

    risa_lib_charlib_string_init(&str);
    str.cap = 8;

    while(size + 1> str.cap)
        str.cap *= 2;

    str.data = RISA_MEM_ALLOC(sizeof(char) * str.cap);

    strncpy(str.data, src + start, size);
    str.data[size] = '\0';
    str.len = size;

    return str;
}

void risa_lib_charlib_string_delete(RisaLibCharlibString* str) {
    RISA_MEM_FREE(str->data);
    risa_lib_charlib_string_init(str);
}

bool risa_lib_charlib_is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool risa_lib_charlib_is_alphascore(char c) {
    return (c >= 'a' && c <= 'z')
           || (c >= 'A' && c <= 'Z')
           || (c == '_');
}

bool risa_lib_charlib_strnicmp(const char* left, const char* right, size_t size) {
    #define LWR(c) (c >= 'A' && c <= 'Z' ? c + 32 : c)

    const char* leftLimit = left + size;

    while(LWR(*left) == LWR(*right) && left < leftLimit) {
        ++left;
        ++right;
    }

    return left == leftLimit;

    #undef LWR
}

bool risa_lib_charlib_strmcmp(const char* left, const char* right, size_t size) {
    while(*left == *right && *left != '\0' && size > 0) {
        ++left;
        ++right;
        --size;
    }

    return (size == 0 && *left == '\0');
}

char* risa_lib_charlib_strdup(const char* src) {
    char* dest = RISA_MEM_ALLOC(strlen(src) + 1);

    strcpy(dest, src);
    return dest;
}

char* risa_lib_charlib_strndup(const char* src, size_t size) {
    char* dest = RISA_MEM_ALLOC(size + 1);

    strncpy(dest, src, size);
    dest[size] = '\0';

    return dest;
}

#undef CHARLIB_STRING_CAP_START