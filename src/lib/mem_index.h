#ifndef MEM_INDEX_H_GUARD
#define MEM_INDEX_H_GUARD

#include "../common/headers.h"

/// Gets the line and column from an index. Counts LF, ignores CR.
void mem_lncol(const char* source, size_t index, size_t* ln, size_t* col);

/// Gets a chunk of memory at an index. The total chunk size is max 2 * chunk + 1. The index position in the chunk is placed in chunkIndex (if pointer is not 0).
char* mem_chunk(const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize);

/// Same as mem_chunk, stops at LF and CR.
char* mem_lnchunk(const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize);

#endif