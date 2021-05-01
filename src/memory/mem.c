#include "mem.h"

#include "../io/log.h"
#include "../def/def.h"

#include <stdlib.h>

#define MEM_BLOCK_START_SIZE 8

#ifdef DEBUG_TRACE_MEMORY_OPS
    static size_t unfreedAllocations = 0;
#endif

void* risa_mem_alloc(size_t size, const char* file, uint32_t line) {
    void* ptr = malloc(size);

    if(ptr == NULL)
        risa_mem_panic();

    #ifdef DEBUG_TRACE_MEMORY_OPS
        ++unfreedAllocations;
        RISA_MEMORY("+ %p                 %-8zu     at line %u in %s", ptr, unfreedAllocations, line, file);
    #endif

    return ptr;
}

void* risa_mem_realloc(void* ptr, size_t size, size_t unitSize, const char* file, uint32_t line) {
    void* newPtr = realloc(ptr, size * unitSize);

    if(newPtr == NULL)
        risa_mem_panic();

    #ifdef DEBUG_TRACE_MEMORY_OPS
        if(ptr == NULL) {
            ++unfreedAllocations;
            RISA_MEMORY("+ %p                 %-8zu     at line %u in %s", newPtr, unfreedAllocations, line, file);
        } else {
            RISA_MEMORY("~ %p -> %p     %-8zu     at line %u in %s", ptr, newPtr, unfreedAllocations, line, file);
        }
    #endif

    return newPtr;
}

void* risa_mem_expand(void* ptr, size_t* size, size_t unitSize, const char* file, uint32_t line) {
    if(*size < MEM_BLOCK_START_SIZE)
        *size = MEM_BLOCK_START_SIZE;
    else (*size) *= 2;

    return risa_mem_realloc(ptr, *size, unitSize, file, line);
}

void risa_mem_free(void* ptr, const char* file, uint32_t line) {
    free(ptr);

    #ifdef DEBUG_TRACE_MEMORY_OPS
        if(ptr != NULL) {
            --unfreedAllocations;
            RISA_MEMORY("- %p                 %-8zu     at line %u in %s", ptr, unfreedAllocations, line, file);
        }
    #endif
}

void risa_mem_panic() {
    exit(RISA_EXIT_OOM);
}

#undef MEM_BLOCK_START_SIZE
