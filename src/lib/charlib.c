#include "charlib.h"
#include "../memory/mem.h"

#include <string.h>

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
