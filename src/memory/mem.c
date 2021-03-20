#include "mem.h"

#include "../io/log.h"

#include <stdlib.h>

#define MEM_BLOCK_START_SIZE 8

void* mem_alloc(size_t size, const char* file, uint32_t line) {
    void* ptr = malloc(size);

    if(ptr == NULL)
        mem_panic();

    RISA_MEMORY("+ %p                 at line %u in %s", ptr, line, file);

    return ptr;
}

void* mem_realloc(void* ptr, size_t size, size_t unitSize, const char* file, uint32_t line) {
    void* newPtr = realloc(ptr, size * unitSize);

    if(newPtr == NULL)
        mem_panic();

    RISA_MEMORY("~ %p -> %p     at line %u in %s", ptr, newPtr, line, file);

    return newPtr;
}

void* mem_expand(void* ptr, size_t* size, size_t unitSize, const char* file, uint32_t line) {
    if(*size < MEM_BLOCK_START_SIZE)
        *size = MEM_BLOCK_START_SIZE;
    else (*size) *= 2;

    return mem_realloc(ptr, *size, unitSize, file, line);
}

void mem_free(void* ptr, const char* file, uint32_t line) {
    if(ptr != NULL)
        free(ptr);

    RISA_MEMORY("- %p                 at line %u in %s", ptr, line, file);
}

void mem_panic() {
    exit(RISA_EXIT_OOM);
}

#undef MEM_BLOCK_START_SIZE
