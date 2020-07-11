#ifndef RISA_CHUNK_H
#define RISA_CHUNK_H

#include "../common/headers.h"
#include "../value/value.h"

typedef struct {
    size_t size;
    size_t capacity;

    uint8_t* bytecode;
    uint32_t* indices;

    ValueArray constants;
} Chunk;

void   chunk_init(Chunk* chunk);
void   chunk_write(Chunk* chunk, uint8_t byte, uint32_t index);
size_t chunk_write_constant(Chunk* chunk, Value constant);
void   chunk_delete(Chunk* chunk);

#endif
