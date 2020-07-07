#ifndef RISA_CHUNK_H
#define RISA_CHUNK_H

#include "../common/headers.h"
#include "../value/value.h"

typedef struct {
    size_t size;
    size_t capacity;

    size_t* indices;

    uint8_t* bytecode;

    ValueArray constants;
} Chunk;

Chunk* chunk_create();
void   chunk_init(Chunk* chunk);
void   chunk_write(Chunk* chunk, uint8_t byte, uint32_t index);
size_t chunk_write_constant(Chunk* chunk, Value constant);
void   chunk_delete(Chunk* chunk);

#endif
