#include "chunk.h"

#include "../memory/mem.h"

void chunk_init(Chunk* chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->bytecode = NULL;
    chunk->indices = NULL;

    value_array_init(&chunk->constants);
}

void chunk_write(Chunk* chunk, uint8_t byte, uint32_t index) {
    while(chunk->capacity <= chunk->size) {
        chunk->bytecode = (uint8_t*) RISA_MEM_EXPAND(chunk->bytecode, &chunk->capacity, sizeof(uint8_t));
        chunk->indices = (uint32_t*) RISA_MEM_REALLOC(chunk->indices, chunk->capacity, sizeof(uint32_t));
    }

    chunk->bytecode[chunk->size] = byte;
    chunk->indices[chunk->size] = index;
    ++chunk->size;
}

size_t chunk_write_constant(Chunk* chunk, Value constant) {
    for(uint32_t i = 0; i < chunk->constants.size; ++i)
        if(value_strict_equals(constant, chunk->constants.values[i]))
            return i;

    value_array_write(&chunk->constants, constant);
    return chunk->constants.size - 1;
}

void chunk_delete(Chunk* chunk) {
    RISA_MEM_FREE(chunk->bytecode);
    RISA_MEM_FREE(chunk->indices);

    value_array_delete(&chunk->constants);

    chunk_init(chunk);
}
