#ifndef RISA_MEM_H_GUARD
#define RISA_MEM_H_GUARD

#include "../def/types.h"

#define RISA_EXIT_OOM 137

#define RISA_MEM_ALLOC(size) mem_alloc(size, __FILE__, __LINE__)
#define RISA_MEM_REALLOC(ptr, size, unitSize) mem_realloc(ptr, size, unitSize, __FILE__, __LINE__)
#define RISA_MEM_EXPAND(ptr, size, unitSize) mem_expand(ptr, size, unitSize, __FILE__, __LINE__)
#define RISA_MEM_FREE(ptr) mem_free(ptr, __FILE__, __LINE__)

void* mem_alloc(size_t size, const char* file, uint32_t line);
void* mem_realloc(void* ptr, size_t size, size_t unitSize, const char* file, uint32_t line);
void* mem_expand(void* ptr, size_t* size, size_t unitSize, const char* file, uint32_t line);
void  mem_free(void* ptr, const char* file, uint32_t line);

void  mem_panic();

#endif
