#include "disassembler.h"

#include "../chunk/bytecode.h"

#include "../common/logging.h"

size_t disassemble_simple_instruction(const char* name, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);

size_t debug_disassemble_instruction(Chunk* chunk, size_t offset) {
    if(offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        PRINT("%04zu ", offset);
        PRINT("   . ");
    } else {
        PRINT("\nINDX LINE OP\n");
        PRINT("%04zu ", offset);
        PRINT("%4zu ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->bytecode[offset];

    switch(instruction) {
        case OP_CNST:
            return disassemble_constant_instruction("CNST", chunk, offset);
        case OP_ADD:
            return disassemble_simple_instruction("ADD", offset);
        case OP_SUB:
            return disassemble_simple_instruction("SUB", offset);
        case OP_MUL:
            return disassemble_simple_instruction("MUL", offset);
        case OP_DIV:
            return disassemble_simple_instruction("DIV", offset);
        case OP_NEG:
            return disassemble_simple_instruction("NEG", offset);
        case OP_RET:
            return disassemble_simple_instruction("RET", offset);
        default:
            return offset;
    }
}

void debug_disassemble_chunk(Chunk* chunk) {
    for(size_t offset = 0; offset < chunk->size;)
        offset = debug_disassemble_instruction(chunk, offset);
}

size_t disassemble_simple_instruction(const char* name, size_t offset) {
    PRINT("%s\n", name);
    return offset + 1;
}

size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    uint8_t index = chunk->bytecode[offset + 1];

    PRINT("%-16s %4d '", name, index);
    value_print(chunk->constants.values[index]);
    PRINT("'\n");

    return offset + 2;
}