#include "chunk.h"

#include "../memory/mem.h"

Chunk* chunk_create() {
    Chunk* chunk = mem_alloc(sizeof(Chunk));
    chunk_init(chunk);

    return chunk;
}

void chunk_init(Chunk* chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->bytecode = NULL;
    chunk->indices = NULL;

    value_array_init(&chunk->constants);
}

void chunk_write(Chunk* chunk, uint8_t byte, uint32_t index) {
    if(chunk->capacity <= chunk->size) {
        chunk->bytecode = mem_expand(chunk->bytecode, &chunk->capacity);
        chunk->indices = mem_realloc(chunk->indices, chunk->capacity * sizeof(uint32_t));
    }

    chunk->bytecode[chunk->size] = byte;
    chunk->indices[chunk->size] = index;
    ++chunk->size;
}

size_t chunk_write_constant(Chunk* chunk, Value constant) {
    value_array_write(&chunk->constants, constant);
    return chunk->constants.size - 1;
}

void chunk_delete(Chunk* chunk) {
    mem_free(chunk->bytecode);
    value_array_delete(&chunk->constants);

    chunk_init(chunk);
}
