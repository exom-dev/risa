#include "mem.h"

#include "../common/logging.h"

#include <stdlib.h>

#define MEM_BLOCK_START_SIZE 8

void* mem_alloc(size_t size) {
    void* ptr = malloc(size);

    if(ptr == NULL)
        mem_panic();
    return ptr;
}

void* mem_realloc(void* ptr, size_t size) {
    void* newPtr = realloc(ptr, size);

    if(newPtr == NULL)
        mem_panic();
    return newPtr;
}

void* mem_expand(void* ptr, size_t* size) {
    if(*size < MEM_BLOCK_START_SIZE)
        *size = MEM_BLOCK_START_SIZE;
    else *size *= 2;

    void* newPtr = realloc(ptr, *size);

    if(newPtr == NULL)
        mem_panic();
    return newPtr;
}

void mem_free(void* ptr) {
    if(ptr != NULL)
        free(ptr);
}

void mem_destroy() {
    WARNING("Mem Destroy");
}

void mem_panic() {
    mem_destroy();
    TERMINATE(70, "Process out of memory");
}
