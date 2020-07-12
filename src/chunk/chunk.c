#include "chunk.h"

#include "../memory/mem.h"
#include "../common/logging.h"

void chunk_init(Chunk* chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->bytecode = NULL;
    chunk->indices = NULL;

    value_array_init(&chunk->constants);
}

void chunk_write(Chunk* chunk, uint8_t byte, uint32_t index) {
    while(chunk->capacity <= chunk->size) {
        chunk->bytecode = (uint8_t*) MEM_EXPAND(chunk->bytecode, &chunk->capacity, sizeof(uint8_t));
        chunk->indices = (uint32_t*) MEM_REALLOC(chunk->indices, chunk->capacity, sizeof(uint32_t));
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
    MEM_FREE(chunk->bytecode);
    MEM_FREE(chunk->indices);

    value_array_delete(&chunk->constants);

    chunk_init(chunk);
}
