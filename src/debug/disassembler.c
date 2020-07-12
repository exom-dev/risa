#include "disassembler.h"

#include "../chunk/bytecode.h"

#include "../common/logging.h"

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_unary_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_producer_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_simple_instruction(const char* name, size_t offset);

size_t debug_disassemble_instruction(Chunk* chunk, size_t offset) {
    if(offset > 0 && chunk->indices[offset] == chunk->indices[offset - 1]) {
        PRINT("%04zu ", offset);
        PRINT("   . ");
    } else {
        PRINT("\nOFFS INDX OP\n");
        PRINT("%04zu ", offset);
        PRINT("%4u ", chunk->indices[offset]);
    }

    uint8_t instruction = chunk->bytecode[offset];

    switch(instruction) {
        case OP_CNST:
            return disassemble_constant_instruction("CNST", chunk, offset);
        case OP_NULL:
            return disassemble_producer_instruction("NULL", chunk, offset);
        case OP_TRUE:
            return disassemble_producer_instruction("TRUE", chunk, offset);
        case OP_FALSE:
            return disassemble_producer_instruction("FALSE", chunk, offset);
        case OP_NOT:
            return disassemble_unary_instruction("NOT", chunk, offset);
        case OP_INV:
            return disassemble_unary_instruction("INV", chunk, offset);
        case OP_NEG:
            return disassemble_unary_instruction("NEG", chunk, offset);
        case OP_ADD:
            return disassemble_arithmetic_instruction("ADD", chunk, offset);
        case OP_SUB:
            return disassemble_arithmetic_instruction("SUB", chunk, offset);
        case OP_MUL:
            return disassemble_arithmetic_instruction("MUL", chunk, offset);
        case OP_DIV:
            return disassemble_arithmetic_instruction("DIV", chunk, offset);
        case OP_EQ:
            return disassemble_arithmetic_instruction("EQ", chunk, offset);
        case OP_NEQ:
            return disassemble_arithmetic_instruction("NEQ", chunk, offset);
        case OP_GT:
            return disassemble_arithmetic_instruction("GT", chunk, offset);
        case OP_GTE:
            return disassemble_arithmetic_instruction("GTE", chunk, offset);
        case OP_LT:
            return disassemble_arithmetic_instruction("LT", chunk, offset);
        case OP_LTE:
            return disassemble_arithmetic_instruction("LTE", chunk, offset);
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

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 3]);
    return offset + 4;
}

size_t disassemble_unary_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    return offset + 3;
}

size_t disassemble_producer_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d\n", name, chunk->bytecode[offset + 1]);

    return offset + 2;
}

size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 2]]);
    PRINT("'\n");

    return offset + 3;
}

size_t disassemble_simple_instruction(const char* name, size_t offset) {
    PRINT("%s\n", name);
    return offset + 1;
}