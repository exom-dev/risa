#ifndef RISA_MACRO_H_GUARD
#define RISA_MACRO_H_GUARD

#include <stdlib.h>

#if defined(_MSC_VER)
    #define COMPILER_MSVC
#elif defined(__GNUC__)
    #define COMPILER_GCC
#else
    #define COMPILER_UNKNOWN
#endif

#ifndef NDEBUG
    #define DEBUG
#endif

#define RISA_STRINGIFY_DIRECTLY(str) #str
#define RISA_STRINGIFY(str) RISA_STRINGIFY_DIRECTLY(str)

#define RISA_CONCAT_DIRECTLY(left, right)  left##right
#define RISA_CONCAT(left, right) RISA_CONCAT_DIRECTLY(left, right)

#ifdef DEBUG
    #define RISA_PANIC(fmt, ...)                                                                 \
        do {                                                                                     \
            fprintf(stderr, "[PANIC] %s at line %d\n\t" fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
            abort();                                                                             \
        } while(false)
#else
    #define RISA_PANIC(fmt, ...)
#endif

#endif