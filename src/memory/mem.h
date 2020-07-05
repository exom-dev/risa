#ifndef RISA_MEM_H_GUARD
#define RISA_MEM_H_GUARD

#include "../common/headers.h"

void* mem_alloc(size_t size);
void* mem_realloc(void* ptr, size_t size);
void* mem_expand(void* ptr, size_t* size);
void  mem_free(void* ptr);

void  mem_destroy();
void  mem_panic();

#endif
