#include "mem_index.h"

#include "../memory/mem.h"

#include <string.h>
#include <stdlib.h>

void mem_lncol(const char* source, size_t index, size_t* ln, size_t* col) {
    size_t  l = 1;
    size_t  c = 1;
    char current;

    const char* limit = source + index;

    while(source < limit) {
        current = *(source++);

        if(current == '\n') {
            ++l;
            c = 1;
        } else if(current != '\r') {
            ++c;
        }
    }

    *ln = l;
    *col = c;
}

char* mem_chunk(const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize) {
    size_t startIndex;
    size_t endIndex;

    if(index >= chunk)
        startIndex = index - chunk;
    else startIndex = 0;

    if(chunkIndex != 0)
        *chunkIndex = index - startIndex;

    endIndex = index + chunk;
    
    if(endIndex > size)
        endIndex = size;

    // Re-use this variable instead of creating a new one.
    chunk = (endIndex - startIndex + 1);

    if(chunkSize != 0)
        *chunkSize = chunk;

    char* data = (char*) mem_alloc(chunk);
    memcpy(data, source + startIndex, chunk);

    return data;
}

char* mem_lnchunk(const char* source, size_t index, size_t size, size_t chunk, size_t* chunkIndex, size_t* chunkSize) {
    const char* start = source + index;
    const char* end = source + index;

    const char* limit;

    if(index > chunk)
        limit = source + index - chunk;
    else limit = source;

    while(start > limit && *start != '\n' && *start != '\r')
        --start;

    if(*start == '\n' || *start == '\r')
        ++start;

    if(chunkIndex != 0)
        *chunkIndex = source + index - start;

    if(index + chunk < size)
        limit = source + index + chunk;
    else limit = source + size - 1;

    while(end < limit && *end != '\n' && *end != '\r')
        ++end;

    if(*end == '\n' || *end == '\r')
        --end;

    // Re-use this variable instead of creating a new one.
    chunk = (end - start + 1);

    if(chunkSize != 0)
        *chunkSize = chunk;

    char* data;
    data = (char*) mem_alloc(chunk);
    memcpy(data, start, chunk);

    return data;
}