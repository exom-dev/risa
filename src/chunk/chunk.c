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
    chunk->lines = NULL;

    value_array_init(&chunk->constants);
}

void chunk_write(Chunk* chunk, uint8_t byte, uint32_t line) {
    if(chunk->capacity <= chunk->size) {
        chunk->bytecode = mem_expand(chunk->bytecode, &chunk->capacity);
        chunk->lines = mem_realloc(chunk->lines, chunk->capacity * sizeof(uint32_t));
    }

    chunk->bytecode[chunk->size] = byte;
    chunk->lines[chunk->size] = line;
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
