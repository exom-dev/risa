#ifndef RISA_LOGGING_H_GUARD
#define RISA_LOGGING_H_GUARD

#include <stdio.h>
#include <stdarg.h>

#define PRINT(fmt, ...) \
    fprintf(stdout, fmt, ##__VA_ARGS__)
#define WARNING(fmt, ...) \
    fprintf(stdout, "[warning] " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) \
    fprintf(stderr, "[error] " fmt "\n", ##__VA_ARGS__ )

#define TERMINATE(code, fmt, ...) \
    do {\
        fprintf(stderr, "[error] " fmt "\n", ##__VA_ARGS__ ); \
        exit(code); \
    } while(0)

#endif
