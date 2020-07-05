#include "disassembler.h"

#include "../chunk/bytecode.h"

#include <stdio.h>

size_t disassemble_instruction(Chunk* chunk, size_t offset);
size_t disassemble_simple_instruction(const char* name, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);

void risa_disassemble_chunk(Chunk* chunk) {
    for(size_t offset = 0; offset < chunk->size;)
        offset = disassemble_instruction(chunk, offset);
}

size_t disassemble_instruction(Chunk* chunk, size_t offset) {
    if(offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("%04d ", offset);
        printf("   . ");
    } else {
        printf("\nINDX LINE OP\n");
        printf("%04d ", offset);
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->bytecode[offset];

    switch(instruction) {
        case OP_CONSTANT:
            return disassemble_constant_instruction("CONSTANT", chunk, offset);
        case OP_RETURN:
            return disassemble_simple_instruction("RETURN", offset);
    }
}

size_t disassemble_simple_instruction(const char* name, size_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    uint8_t index = chunk->bytecode[offset + 1];

    printf("%-16s %4d '", name, index);
    value_print(chunk->constants.values[index]);
    printf("'\n");

    return offset + 2;
}