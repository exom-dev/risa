#include "disassembler.h"

#include "../chunk/bytecode.h"

#include "../common/logging.h"

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);

size_t debug_disassemble_instruction(Chunk* chunk, size_t offset) {
    if(offset > 0 && chunk->indices[offset] == chunk->indices[offset - 1]) {
        PRINT("%04zu ", offset);
        PRINT("   . ");
    } else {
        PRINT("\nINDX LINE OP\n");
        PRINT("%04zu ", offset);
        PRINT("%4zu ", chunk->indices[offset]);
    }

    uint8_t instruction = chunk->bytecode[offset];

    switch(instruction) {
        case OP_CNST:
            return disassemble_constant_instruction("CNST", chunk, offset);
        case OP_ADD:
            return disassemble_arithmetic_instruction("ADD", chunk, offset);
        case OP_SUB:
            return disassemble_arithmetic_instruction("SUB", chunk, offset);
        case OP_MUL:
            return disassemble_arithmetic_instruction("MUL", chunk, offset);
        case OP_DIV:
            return disassemble_arithmetic_instruction("DIV", chunk, offset);
        case OP_NEG:
            return disassemble_arithmetic_instruction("NEG", chunk, offset);
        case OP_RET:
            return disassemble_arithmetic_instruction("RET", chunk, offset);
        default:
            return offset;
    }
}

void debug_disassemble_chunk(Chunk* chunk) {
    for(size_t offset = 0; offset < chunk->size;)
        offset = debug_disassemble_instruction(chunk, offset);
}

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 3]);
    return offset + 4;
}

size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 1]]);
    PRINT("'\n");

    return offset + 3;
}