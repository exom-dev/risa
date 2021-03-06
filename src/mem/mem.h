#ifndef RISA_MEM_H_GUARD
#define RISA_MEM_H_GUARD

#include "../api.h"
#include "../def/types.h"

#define RISA_EXIT_OOM 137

#define RISA_MEM_ALLOC(size) risa_mem_alloc(size, __FILE__, __LINE__)
#define RISA_MEM_REALLOC(ptr, size, unitSize) risa_mem_realloc(ptr, size, unitSize, __FILE__, __LINE__)
#define RISA_MEM_EXPAND(ptr, size, unitSize) risa_mem_expand(ptr, size, unitSize, __FILE__, __LINE__)
#define RISA_MEM_FREE(ptr) risa_mem_free(ptr, __FILE__, __LINE__)

RISA_API void* risa_mem_alloc   (uint32_t size, const char* file, uint32_t line);
RISA_API void* risa_mem_realloc (void* ptr, uint32_t size, uint32_t unitSize, const char* file, uint32_t line);
RISA_API void* risa_mem_expand  (void* ptr, uint32_t* size, uint32_t unitSize, const char* file, uint32_t line);
RISA_API void  risa_mem_free    (void* ptr, const char* file, uint32_t line);

RISA_API void  risa_mem_panic   ();

#endif
