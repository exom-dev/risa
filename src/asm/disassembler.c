#include "disassembler.h"

#include "../chunk/bytecode.h"
#include "../io/log.h"
#include "../value/dense.h"

#define RISA_DISASM_CHUNK (disassembler->chunk)
#define RISA_DISASM_OFFSET (disassembler->offset)

static void disassemble_unary_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassemble_binary_instruction(RisaDisassembler* disassembler, const char *name, uint8_t types);
static void disassemble_byte_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_word_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_constant_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_mov_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_call_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_global_define_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassemble_global_get_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_global_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassemble_upvalue_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_upvalue_get_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_upvalue_set_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_upvalue_close_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_closure_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_array_push_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassemble_array_length_instruction(RisaDisassembler* disassembler, const char* name);
static void disassemble_get_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassemble_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types);
static void disassembler_process_instruction(RisaDisassembler* disassembler);

void disassembler_init(RisaDisassembler* disassembler) {
    risa_io_init(&disassembler->io);
    disassembler->chunk = NULL;
    disassembler->offset = 0;
}

void disassembler_load(RisaDisassembler* disassembler, Chunk* chunk) {
    disassembler->chunk = chunk;
}

void disassembler_run(RisaDisassembler* disassembler) {
    if(disassembler->chunk != NULL) {
        RISA_OUT(disassembler->io, "\nOFFS INDX OP\n");

        while(disassembler->offset < disassembler->chunk->size)
            disassembler_process_instruction(disassembler);

        // Decompile all of the functions contained in the chunk.
        for(size_t i = 0; i < disassembler->chunk->constants.size; ++i) {
            DenseFunction* function;

            if(value_is_dense_of_type(disassembler->chunk->constants.values[i], DVAL_FUNCTION))
                function = AS_FUNCTION(disassembler->chunk->constants.values[i]);
            else if(value_is_dense_of_type(disassembler->chunk->constants.values[i], DVAL_CLOSURE))
                function = AS_CLOSURE(disassembler->chunk->constants.values[i])->function;
            else continue;

            RISA_OUT(disassembler->io, "\n<%s>", function->name->chars);

            RisaDisassembler disasm;
            disassembler_init(&disasm);
            risa_io_clone(&disasm.io, &disassembler->io);

            disassembler_load(&disasm, &function->chunk);
            disassembler_run(&disasm);
        }
    }
}

void disassembler_reset(RisaDisassembler* disassembler) {
    disassembler->offset = 0;
}

static void disassembler_process_instruction(RisaDisassembler* disassembler) {
    RISA_OUT(disassembler->io, "%04zu %4u ", RISA_DISASM_OFFSET, RISA_DISASM_CHUNK->indices[RISA_DISASM_OFFSET]);

    uint8_t instruction = RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET];
    uint8_t types = instruction & RISA_TODLR_TYPE_MASK;
    instruction &= RISA_TODLR_INSTRUCTION_MASK;

    switch(instruction) {
        case OP_CNST:
            disassemble_constant_instruction(disassembler, "CNST");
            break;
        case OP_CNSTW:
            disassemble_constant_instruction(disassembler, "CNSTW");
            break;
        case OP_MOV:
            disassemble_mov_instruction(disassembler, "MOV");
            break;
        case OP_CLONE:
            disassemble_mov_instruction(disassembler, "CLONE");
            break;
        case OP_DGLOB:
            disassemble_global_define_instruction(disassembler, "DGLOB", types);
            break;
        case OP_GGLOB:
            disassemble_global_get_instruction(disassembler, "GGLOB");
            break;
        case OP_SGLOB:
            disassemble_global_set_instruction(disassembler, "SGLOB", types);
            break;
        case OP_UPVAL:
            disassemble_upvalue_instruction(disassembler, "UPVAL");
            break;
        case OP_GUPVAL:
            disassemble_upvalue_get_instruction(disassembler, "GUPVAL");
            break;
        case OP_SUPVAL:
            disassemble_upvalue_set_instruction(disassembler, "SUPVAL");
            break;
        case OP_CUPVAL:
            disassemble_upvalue_close_instruction(disassembler, "CUPVAL");
            break;
        case OP_CLSR:
            disassemble_closure_instruction(disassembler, "CLSR");
            break;
        case OP_ARR:
            disassemble_byte_instruction(disassembler, "ARR");
            break;
        case OP_PARR:
            disassemble_array_push_instruction(disassembler, "PARR", types);
            break;
        case OP_LEN:
            disassemble_array_length_instruction(disassembler, "LEN");
            break;
        case OP_OBJ:
            disassemble_byte_instruction(disassembler, "OBJ");
            break;
        case OP_GET:
            disassemble_get_instruction(disassembler, "GET", types);
            break;
        case OP_SET:
            disassemble_set_instruction(disassembler, "SET", types);
            break;
        case OP_NULL:
            disassemble_byte_instruction(disassembler, "NULL");
            break;
        case OP_TRUE:
            disassemble_byte_instruction(disassembler, "TRUE");
            break;
        case OP_FALSE:
            disassemble_byte_instruction(disassembler, "FALSE");
            break;
        case OP_NOT:
            disassemble_unary_instruction(disassembler, "NOT", types);
            break;
        case OP_BNOT:
            disassemble_unary_instruction(disassembler, "BNOT", types);
            break;
        case OP_NEG:
            disassemble_unary_instruction(disassembler, "NEG", types);
            break;
        case OP_INC:
            disassemble_byte_instruction(disassembler, "INC");
            break;
        case OP_DEC:
            disassemble_byte_instruction(disassembler, "DEC");
            break;
        case OP_ADD:
            disassemble_binary_instruction(disassembler, "ADD", types);
            break;
        case OP_SUB:
            disassemble_binary_instruction(disassembler, "SUB", types);
            break;
        case OP_MUL:
            disassemble_binary_instruction(disassembler, "MUL", types);
            break;
        case OP_DIV:
            disassemble_binary_instruction(disassembler, "DIV", types);
            break;
        case OP_MOD:
            disassemble_binary_instruction(disassembler, "MOD", types);
            break;
        case OP_SHL:
            disassemble_binary_instruction(disassembler, "SHL", types);
            break;
        case OP_SHR:
            disassemble_binary_instruction(disassembler, "SHR", types);
            break;
        case OP_LT:
            disassemble_binary_instruction(disassembler, "LT", types);
            break;
        case OP_LTE:
            disassemble_binary_instruction(disassembler, "LTE", types);
            break;
        case OP_EQ:
            disassemble_binary_instruction(disassembler, "EQ", types);
            break;
        case OP_NEQ:
            disassemble_binary_instruction(disassembler, "NEQ", types);
            break;
        case OP_BAND:
            disassemble_binary_instruction(disassembler, "BAND", types);
            break;
        case OP_BXOR:
            disassemble_binary_instruction(disassembler, "BXOR", types);
            break;
        case OP_BOR:
            disassemble_binary_instruction(disassembler, "BOR", types);
            break;
        case OP_TEST:
            disassemble_byte_instruction(disassembler, "TEST");
            break;
        case OP_NTEST:
            disassemble_byte_instruction(disassembler, "NTEST");
            break;
        case OP_JMP:
            disassemble_byte_instruction(disassembler, "JMP");
            break;
        case OP_JMPW:
            disassemble_word_instruction(disassembler, "JMPW");
            break;
        case OP_BJMP:
            disassemble_byte_instruction(disassembler, "BJMP");
            break;
        case OP_BJMPW:
            disassemble_word_instruction(disassembler, "BJMPW");
            break;
        case OP_CALL:
            disassemble_call_instruction(disassembler, "CALL");
            break;
        case OP_RET:
            disassemble_byte_instruction(disassembler, "RET");
            break;
        case OP_ACC:
            disassemble_byte_instruction(disassembler, "ACC");
            break;
        case OP_DIS:
            disassemble_byte_instruction(disassembler, "DIS");
            break;
        default:
            RISA_OUT(disassembler->io, "<UNK>");
    }

    RISA_DISASM_OFFSET += RISA_TODLR_INSTRUCTION_SIZE;
}

void disassemble_binary_instruction(RisaDisassembler* disassembler, const char *name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c %4d%c\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'),
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 3],
             (types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r'));
}

void disassemble_unary_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));
}

void disassemble_byte_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1]);
}

void disassemble_word_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %5hu\n", name,
             (uint16_t) RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1]);
}

void disassemble_constant_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    '", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);

    value_print(&disassembler->io, RISA_DISASM_CHUNK->constants.values[RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]]);

    RISA_OUT(disassembler->io, "'\n");
}

void disassemble_mov_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);
}

void disassemble_call_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);
}

void disassemble_global_define_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c    '", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));

    value_print(&disassembler->io, RISA_DISASM_CHUNK->constants.values[RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1]]);

    RISA_OUT(disassembler->io, "'\n");
}

void disassemble_global_get_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    '", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);

    value_print(&disassembler->io, RISA_DISASM_CHUNK->constants.values[RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]]);

    RISA_OUT(disassembler->io, "'\n");
}

void disassemble_global_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c    '", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));

    value_print(&disassembler->io, RISA_DISASM_CHUNK->constants.values[RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1]]);

    RISA_OUT(disassembler->io, "'\n");
}

void disassemble_upvalue_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    %s\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2] == 0 ? "upvalue" : "local");
}

void disassemble_upvalue_get_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);
}

void disassemble_upvalue_set_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);
}

void disassemble_upvalue_close_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1]);
}

void disassemble_closure_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d %4d   '", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 3]);

    value_print(&disassembler->io, RISA_DISASM_CHUNK->constants.values[RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]]);

    RISA_OUT(disassembler->io, "'\n");
}

void disassemble_array_push_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));
}

void disassemble_array_length_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name, RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1], RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2]);
}

void disassemble_get_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d %4d%c\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 3],
             (types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r'));
}

void disassemble_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c %4d%c\n", name,
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 2],
             types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r',
             RISA_DISASM_CHUNK->bytecode[RISA_DISASM_OFFSET + 3],
             types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r');
}

#undef RISA_DISASM_OFFSET
#undef RISA_DISASM_CHUNK