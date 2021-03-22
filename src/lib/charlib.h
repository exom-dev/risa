#ifndef RISA_LIB_CHARLIB_H_GUARD
#define RISA_LIB_CHARLIB_H_GUARD

#include "../api.h"
#include "../def/types.h"

RISA_API_HIDDEN bool risa_lib_charlib_is_digit(char c);
RISA_API_HIDDEN bool risa_lib_charlib_is_alphascore(char c);
RISA_API_HIDDEN bool risa_lib_charlib_strnicmp(const char* left, const char* right, size_t size);
RISA_API_HIDDEN bool risa_lib_charlib_strmcmp(const char* left, const char* right, size_t size); // Like strcmp, but right is not null-terminated, while left is. Size = right size.

RISA_API_HIDDEN char* risa_lib_charlib_strdup(const char* src);
RISA_API_HIDDEN char* risa_lib_charlib_strndup(const char* src, size_t size);

#endif
