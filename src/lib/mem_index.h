#ifndef RISA_LIB_MEM_INDEX_H_GUARD
#define RISA_LIB_MEM_INDEX_H_GUARD

#include "../api.h"
#include "../def/types.h"

/// Gets the line and column from an index. Counts LF, ignores CR.
RISA_API_HIDDEN void risa_lib_mem_lncol    (const char* source, size_t index, size_t* ln, size_t* col);

/// Gets a cluster of memory at an index. The total cluster size is max 2 * cluster + 1. The index position in the cluster is placed in chunkIndex (if pointer is not 0).
RISA_API_HIDDEN char* risa_lib_mem_chunk   (const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize);

/// Same as risa_lib_mem_chunk, stops at LF and CR.
RISA_API_HIDDEN char* risa_lib_mem_lnchunk (const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize);

#endif