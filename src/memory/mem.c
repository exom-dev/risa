#include "mem.h"

#include "../common/logging.h"

#include <stdlib.h>

#define MEM_BLOCK_START_SIZE 8

void* mem_alloc(size_t size, const char* file, uint32_t line) {
    void* ptr = malloc(size);

    if(ptr == NULL)
        mem_panic();

    MEMORY("+ %p                 at line %u in %s", ptr, line, file);

    return ptr;
}

void* mem_realloc(void* ptr, size_t size, size_t unitSize, const char* file, uint32_t line) {
    void* newPtr = realloc(ptr, size * unitSize);

    if(newPtr == NULL)
        mem_panic();

    MEMORY("~ %p -> %p     at line %u in %s", ptr, newPtr, line, file);

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

    MEMORY("- %p                 at line %u in %s", ptr, line, file);
}

void mem_destroy() {
    VERBOSE("Mem Destroy");
}

void mem_panic() {
    mem_destroy();
    TERMINATE(70, "Process out of memory");
}
