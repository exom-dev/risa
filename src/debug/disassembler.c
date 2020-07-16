#include "disassembler.h"

#include "../chunk/bytecode.h"

#include "../common/logging.h"

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_unary_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_byte_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_word_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_mov_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_call_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_global_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_global_set_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_return_instruction(const char* name, Chunk* chunk, size_t offset);

size_t debug_disassemble_instruction(Chunk* chunk, size_t offset) {
    PRINT("%04zu %4u ", offset, chunk->indices[offset]);

    uint8_t instruction = chunk->bytecode[offset];

    switch(instruction) {
        case OP_CNST:
            return disassemble_constant_instruction("CNST", chunk, offset);
        case OP_CNSTW:
            return disassemble_constant_instruction("CNSTW", chunk, offset);
        case OP_MOV:
            return disassemble_mov_instruction("MOV", chunk, offset);
        case OP_DGLOB:
            return disassemble_global_instruction("DGLOB", chunk, offset);
        case OP_GGLOB:
            return disassemble_global_instruction("GGLOB", chunk, offset);
        case OP_SGLOB:
            return disassemble_global_set_instruction("SGLOB", chunk, offset);
        case OP_NULL:
            return disassemble_byte_instruction("NULL", chunk, offset);
        case OP_TRUE:
            return disassemble_byte_instruction("TRUE", chunk, offset);
        case OP_FALSE:
            return disassemble_byte_instruction("FALSE", chunk, offset);
        case OP_NOT:
            return disassemble_unary_instruction("NOT", chunk, offset);
        case OP_BNOT:
            return disassemble_unary_instruction("BNOT", chunk, offset);
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
        case OP_MOD:
            return disassemble_arithmetic_instruction("MOD", chunk, offset);
        case OP_SHL:
            return disassemble_arithmetic_instruction("SHL", chunk, offset);
        case OP_SHR:
            return disassemble_arithmetic_instruction("SHR", chunk, offset);
        case OP_GT:
            return disassemble_arithmetic_instruction("GT", chunk, offset);
        case OP_GTE:
            return disassemble_arithmetic_instruction("GTE", chunk, offset);
        case OP_LT:
            return disassemble_arithmetic_instruction("LT", chunk, offset);
        case OP_LTE:
            return disassemble_arithmetic_instruction("LTE", chunk, offset);
        case OP_EQ:
            return disassemble_arithmetic_instruction("EQ", chunk, offset);
        case OP_NEQ:
            return disassemble_arithmetic_instruction("NEQ", chunk, offset);
        case OP_BAND:
            return disassemble_arithmetic_instruction("BAND", chunk, offset);
        case OP_BXOR:
            return disassemble_arithmetic_instruction("BXOR", chunk, offset);
        case OP_BOR:
            return disassemble_arithmetic_instruction("BOR", chunk, offset);
        case OP_TEST:
            return disassemble_byte_instruction("TEST", chunk, offset);
        case OP_NTEST:
            return disassemble_byte_instruction("NTEST", chunk, offset);
        case OP_JMP:
            return disassemble_byte_instruction("JMP", chunk, offset);
        case OP_JMPW:
            return disassemble_word_instruction("JMPW", chunk, offset);
        case OP_BJMP:
            return disassemble_byte_instruction("BJMP", chunk, offset);
        case OP_BJMPW:
            return disassemble_word_instruction("BJMPW", chunk, offset);
        case OP_CALL:
            return disassemble_call_instruction("CALL", chunk, offset);
        case OP_RET:
            return disassemble_return_instruction("RET", chunk, offset);
        default:
            return offset;
    }
}

void debug_disassemble_chunk(Chunk* chunk) {
    PRINT("\nOFFS INDX OP\n");

    for(size_t offset = 0; offset < chunk->size;)
        offset = debug_disassemble_instruction(chunk, offset);
}

size_t disassemble_arithmetic_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 3]);
    return offset + 4;
}

size_t disassemble_unary_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    return offset + 4;
}

size_t disassemble_byte_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d\n", name, chunk->bytecode[offset + 1]);

    return offset + 4;
}

size_t disassemble_word_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %5hu\n", name, (uint16_t) chunk->bytecode[offset + 1]);

    return offset + 4;
}

size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 2]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_mov_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    return offset + 4;
}

size_t disassemble_call_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    return offset + 4;
}

size_t disassemble_global_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 2]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_global_set_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 1]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_return_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d\n", name, chunk->bytecode[offset + 1]);
    return offset + 4;
}