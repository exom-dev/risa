#ifndef RISA_LIB_CHARLIB_H_GUARD
#define RISA_LIB_CHARLIB_H_GUARD

#include "../api.h"
#include "../def/types.h"

typedef struct {
    size_t len;
    size_t cap;

    char* data;
} RisaLibCharlibString;

RISA_API_HIDDEN void                 risa_lib_charlib_string_init       (RisaLibCharlibString* str);
RISA_API_HIDDEN void                 risa_lib_charlib_string_adjust     (RisaLibCharlibString* str, size_t size);
RISA_API_HIDDEN void                 risa_lib_charlib_string_append     (RisaLibCharlibString* str, RisaLibCharlibString* right);
RISA_API_HIDDEN void                 risa_lib_charlib_string_append_c   (RisaLibCharlibString* str, char* right);
RISA_API_HIDDEN void                 risa_lib_charlib_string_append_chr (RisaLibCharlibString* str, char right);
RISA_API_HIDDEN RisaLibCharlibString risa_lib_charlib_string_from_sub   (const char* src, size_t start, size_t end);
RISA_API_HIDDEN void                 risa_lib_charlib_string_delete     (RisaLibCharlibString* str);

RISA_API_HIDDEN bool risa_lib_charlib_is_digit      (char c);
RISA_API_HIDDEN bool risa_lib_charlib_is_alphascore (char c);
RISA_API_HIDDEN bool risa_lib_charlib_stricmp       (const char* left, const char* right);
RISA_API_HIDDEN bool risa_lib_charlib_strnicmp      (const char* left, const char* right, size_t size);
RISA_API_HIDDEN bool risa_lib_charlib_strmcmp       (const char* left, const char* right, size_t size); // Like strcmp, but right is not null-terminated, while left is. Size = right size.

RISA_API_HIDDEN char* risa_lib_charlib_strdup       (const char* src);
RISA_API_HIDDEN char* risa_lib_charlib_strndup      (const char* src, size_t size);

RISA_API_HIDDEN bool risa_lib_charlib_strtod        (const char* src, double* dest);
RISA_API_HIDDEN bool risa_lib_charlib_strntod       (const char* src, uint32_t size, double* dest);
RISA_API_HIDDEN bool risa_lib_charlib_strtol        (const char* src, int base, int64_t* dest); // TODO: write a strtol function that also determines the base (0x, 0b, ...)
RISA_API_HIDDEN bool risa_lib_charlib_strntol       (const char* src, uint32_t size, int base, int64_t* dest);

#endif
