#include "disassembler.h"

#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../value/dense.h"

#define TYPE_MASK        0xC0
#define INSTRUCTION_MASK 0x3F
#define LEFT_TYPE_MASK   0x80
#define RIGHT_TYPE_MASK  0x40

size_t disassemble_unary_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_binary_instruction(const char *name, uint8_t types, Chunk *chunk, size_t offset);
size_t disassemble_byte_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_word_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_constant_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_mov_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_call_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_global_define_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_global_get_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_global_set_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_upvalue_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_upvalue_get_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_upvalue_set_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_upvalue_close_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_closure_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_array_push_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_array_length_instruction(const char* name, Chunk* chunk, size_t offset);
size_t disassemble_get_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_set_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset);
size_t disassemble_return_instruction(const char* name, Chunk* chunk, size_t offset);

size_t debug_disassemble_instruction(Chunk* chunk, size_t offset) {
    PRINT("%04zu %4u ", offset, chunk->indices[offset]);

    uint8_t instruction = chunk->bytecode[offset];
    uint8_t types = instruction & TYPE_MASK;
    instruction &= INSTRUCTION_MASK;

    switch(instruction) {
        case OP_CNST:
            return disassemble_constant_instruction("CNST", chunk, offset);
        case OP_CNSTW:
            return disassemble_constant_instruction("CNSTW", chunk, offset);
        case OP_MOV:
            return disassemble_mov_instruction("MOV", chunk, offset);
        case OP_DGLOB:
            return disassemble_global_define_instruction("DGLOB", types, chunk, offset);
        case OP_GGLOB:
            return disassemble_global_get_instruction("GGLOB", chunk, offset);
        case OP_SGLOB:
            return disassemble_global_set_instruction("SGLOB", types, chunk, offset);
        case OP_UPVAL:
            return disassemble_upvalue_instruction("UPVAL", chunk, offset);
        case OP_GUPVAL:
            return disassemble_upvalue_get_instruction("GUPVAL", chunk, offset);
        case OP_SUPVAL:
            return disassemble_upvalue_set_instruction("SUPVAL", chunk, offset);
        case OP_CUPVAL:
            return disassemble_upvalue_close_instruction("CUPVAL", chunk, offset);
        case OP_CLSR:
            return disassemble_closure_instruction("CLSR", chunk, offset);
        case OP_ARR:
            return disassemble_byte_instruction("ARR", chunk, offset);
        case OP_PARR:
            return disassemble_array_push_instruction("PARR", types, chunk, offset);
        case OP_LEN:
            return disassemble_array_length_instruction("LEN", chunk, offset);
        case OP_GET:
            return disassemble_get_instruction("GET", types, chunk, offset);
        case OP_SET:
            return disassemble_set_instruction("SET", types, chunk, offset);
        case OP_NULL:
            return disassemble_byte_instruction("NULL", chunk, offset);
        case OP_TRUE:
            return disassemble_byte_instruction("TRUE", chunk, offset);
        case OP_FALSE:
            return disassemble_byte_instruction("FALSE", chunk, offset);
        case OP_NOT:
            return disassemble_unary_instruction("NOT", types, chunk, offset);
        case OP_BNOT:
            return disassemble_unary_instruction("BNOT", types, chunk, offset);
        case OP_NEG:
            return disassemble_unary_instruction("NEG", types, chunk, offset);
        case OP_ADD:
            return disassemble_binary_instruction("ADD", types, chunk, offset);
        case OP_SUB:
            return disassemble_binary_instruction("SUB", types, chunk, offset);
        case OP_MUL:
            return disassemble_binary_instruction("MUL", types, chunk, offset);
        case OP_DIV:
            return disassemble_binary_instruction("DIV", types, chunk, offset);
        case OP_MOD:
            return disassemble_binary_instruction("MOD", types, chunk, offset);
        case OP_SHL:
            return disassemble_binary_instruction("SHL", types, chunk, offset);
        case OP_SHR:
            return disassemble_binary_instruction("SHR", types, chunk, offset);
        case OP_GT:
            return disassemble_binary_instruction("GT", types, chunk, offset);
        case OP_GTE:
            return disassemble_binary_instruction("GTE", types, chunk, offset);
        case OP_LT:
            return disassemble_binary_instruction("LT", types, chunk, offset);
        case OP_LTE:
            return disassemble_binary_instruction("LTE", types, chunk, offset);
        case OP_EQ:
            return disassemble_binary_instruction("EQ", types, chunk, offset);
        case OP_NEQ:
            return disassemble_binary_instruction("NEQ", types, chunk, offset);
        case OP_BAND:
            return disassemble_binary_instruction("BAND", types, chunk, offset);
        case OP_BXOR:
            return disassemble_binary_instruction("BXOR", types, chunk, offset);
        case OP_BOR:
            return disassemble_binary_instruction("BOR", types, chunk, offset);
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
    for(size_t i = 0; i < chunk->constants.size; ++i) {
        if(value_is_dense_of_type(chunk->constants.values[i], DVAL_FUNCTION)) {
            PRINT("\n<%s>", AS_FUNCTION(chunk->constants.values[i])->name->chars);
            debug_disassemble_chunk(&(AS_FUNCTION(chunk->constants.values[i]))->chunk);
        } else if(value_is_dense_of_type(chunk->constants.values[i], DVAL_CLOSURE)) {
            PRINT("\n<%s>", AS_CLOSURE(chunk->constants.values[i])->function->name->chars);
            debug_disassemble_chunk(&(AS_CLOSURE(chunk->constants.values[i]))->function->chunk);
        }
    }
}

size_t disassemble_binary_instruction(const char *name, uint8_t types, Chunk *chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c %4d%c\n", name, chunk->bytecode[offset + 1],chunk->bytecode[offset + 2], (types & LEFT_TYPE_MASK ? 'c' : 'r'), chunk->bytecode[offset + 3], (types & RIGHT_TYPE_MASK ? 'c' : 'r'));
    return offset + 4;
}

size_t disassemble_unary_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], (types & LEFT_TYPE_MASK ? 'c' : 'r'));
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

size_t disassemble_global_define_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], (types & LEFT_TYPE_MASK ? 'c' : 'r'));
    value_print(chunk->constants.values[chunk->bytecode[offset + 1]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_global_get_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 2]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_global_set_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c    '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], (types & LEFT_TYPE_MASK ? 'c' : 'r'));
    value_print(chunk->constants.values[chunk->bytecode[offset + 1]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_upvalue_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d    %s\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 2] == 0 ? "upvalue" : "local");

    return offset + 4;
}

size_t disassemble_upvalue_get_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);

    return offset + 4;
}

size_t disassemble_upvalue_set_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);

    return offset + 4;
}

size_t disassemble_upvalue_close_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d\n", name, chunk->bytecode[offset + 1]);

    return offset + 4;
}

size_t disassemble_closure_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d %4d   '", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 3]);
    value_print(chunk->constants.values[chunk->bytecode[offset + 2]]);
    PRINT("'\n");

    return offset + 4;
}

size_t disassemble_array_push_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], (types & LEFT_TYPE_MASK ? 'c' : 'r'));

    return offset + 4;
}

size_t disassemble_array_length_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2]);

    return offset + 4;
}

size_t disassemble_get_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d %4d%c\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], chunk->bytecode[offset + 3], (types & RIGHT_TYPE_MASK ? 'c' : 'r'));

    return offset + 4;
}

size_t disassemble_set_instruction(const char* name, uint8_t types, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d %4d%c %4d\n", name, chunk->bytecode[offset + 1], chunk->bytecode[offset + 2], types & LEFT_TYPE_MASK ? 'c' : 'r', chunk->bytecode[offset + 3]);

    return offset + 4;
}

size_t disassemble_return_instruction(const char* name, Chunk* chunk, size_t offset) {
    PRINT("%-16s %4d\n", name, chunk->bytecode[offset + 1]);
    return offset + 4;
}

#undef RIGHT_TYPE_MASK
#undef LEFT_TYPE_MASK
#undef INSTRUCTION_MASK
#undef TYPE_MASK