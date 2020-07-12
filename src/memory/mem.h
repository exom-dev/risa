#ifndef RISA_MEM_H_GUARD
#define RISA_MEM_H_GUARD

#include "../common/headers.h"

#define MEM_ALLOC(size) mem_alloc(size, __FILE__, __LINE__)
#define MEM_REALLOC(ptr, size, unitSize) mem_realloc(ptr, size, unitSize, __FILE__, __LINE__)
#define MEM_EXPAND(ptr, size, unitSize) mem_expand(ptr, size, unitSize, __FILE__, __LINE__)
#define MEM_FREE(ptr) mem_free(ptr, __FILE__, __LINE__)

void* mem_alloc(size_t size, const char* file, uint32_t line);
void* mem_realloc(void* ptr, size_t size, size_t unitSize, const char* file, uint32_t line);
void* mem_expand(void* ptr, size_t* size, size_t unitSize, const char* file, uint32_t line);
void  mem_free(void* ptr, const char* file, uint32_t line);

void  mem_destroy();
void  mem_panic();

#endif
