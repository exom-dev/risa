#include "disassembler.h"

#include "../cluster/bytecode.h"
#include "../io/log.h"
#include "../value/dense.h"

#define RISA_DISASM_CLUSTER (disassembler->cluster)
#define RISA_DISASM_OFFSET (disassembler->offset)

static void risa_disassembler_disassemble_unary_instruction         (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_binary_instruction        (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_byte_instruction          (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_acc_instruction           (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_word_instruction          (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_constant_instruction      (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_mov_instruction           (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_call_instruction          (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_global_define_instruction (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_global_get_instruction    (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_global_set_instruction    (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_upvalue_instruction       (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_upvalue_get_instruction   (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_upvalue_set_instruction   (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_upvalue_close_instruction (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_closure_instruction       (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_array_push_instruction    (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_array_length_instruction  (RisaDisassembler*, const char*);
static void risa_disassembler_disassemble_get_instruction           (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassemble_set_instruction           (RisaDisassembler*, const char*, uint8_t);
static void risa_disassembler_disassembler_process_instruction      (RisaDisassembler*);

RisaDisassembler* risa_disassembler_create() {
    RisaDisassembler* disassembler = RISA_MEM_ALLOC(sizeof(RisaDisassembler));

    risa_disassembler_init(disassembler);

    return disassembler;
}

void risa_disassembler_init(RisaDisassembler* disassembler) {
    risa_io_init(&disassembler->io);
    disassembler->cluster = NULL;
    disassembler->offset = 0;
}

void risa_disassembler_load(RisaDisassembler* disassembler, RisaCluster* cluster) {
    disassembler->cluster = cluster;
}

void risa_disassembler_run(RisaDisassembler* disassembler) {
    if(disassembler->cluster != NULL) {
        RISA_OUT(disassembler->io, "\nOFFS INDX OP\n");

        while(disassembler->offset < disassembler->cluster->size) {
            risa_disassembler_disassembler_process_instruction(disassembler);
        }

        // Decompile all of the functions contained in the cluster.
        for(size_t i = 0; i < disassembler->cluster->constants.size; ++i) {
            RisaDenseFunction* function;

            if(risa_value_is_dense_of_type(disassembler->cluster->constants.values[i], RISA_DVAL_FUNCTION))
                function = RISA_AS_FUNCTION(disassembler->cluster->constants.values[i]);
            else if(risa_value_is_dense_of_type(disassembler->cluster->constants.values[i], RISA_DVAL_CLOSURE))
                function = RISA_AS_CLOSURE(disassembler->cluster->constants.values[i])->function;
            else continue;

            RISA_OUT(disassembler->io, "\n<%s>", function->name->chars);

            RisaDisassembler disasm;
            risa_disassembler_init(&disasm);
            risa_io_clone(&disasm.io, &disassembler->io);

            risa_disassembler_load(&disasm, &function->cluster);
            risa_disassembler_run(&disasm);
        }
    }
}

void risa_disassembler_reset(RisaDisassembler* disassembler) {
    disassembler->offset = 0;
}

void risa_disassembler_delete(RisaDisassembler* disassembler) {
    risa_disassembler_init(disassembler);
}

void risa_disassembler_free(RisaDisassembler* disassembler) {
    risa_disassembler_delete(disassembler);

    RISA_MEM_FREE(disassembler);
}

static void risa_disassembler_disassembler_process_instruction(RisaDisassembler* disassembler) {
    RISA_OUT(disassembler->io, "%04u %4u ", RISA_DISASM_OFFSET, RISA_DISASM_CLUSTER->indices[RISA_DISASM_OFFSET]);

    uint8_t instruction = RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET];
    uint8_t types = instruction & RISA_TODLR_TYPE_MASK;
    instruction &= RISA_TODLR_INSTRUCTION_MASK;

    switch(instruction) {
        case RISA_OP_CNST:
            risa_disassembler_disassemble_constant_instruction(disassembler, "CNST");
            break;
        case RISA_OP_CNSTW:
            risa_disassembler_disassemble_constant_instruction(disassembler, "CNSTW");
            break;
        case RISA_OP_MOV:
            risa_disassembler_disassemble_mov_instruction(disassembler, "MOV");
            break;
        case RISA_OP_CLONE:
            risa_disassembler_disassemble_mov_instruction(disassembler, "CLONE");
            break;
        case RISA_OP_DGLOB:
            risa_disassembler_disassemble_global_define_instruction(disassembler, "DGLOB", types);
            break;
        case RISA_OP_GGLOB:
            risa_disassembler_disassemble_global_get_instruction(disassembler, "GGLOB");
            break;
        case RISA_OP_SGLOB:
            risa_disassembler_disassemble_global_set_instruction(disassembler, "SGLOB", types);
            break;
        case RISA_OP_UPVAL:
            risa_disassembler_disassemble_upvalue_instruction(disassembler, "UPVAL");
            break;
        case RISA_OP_GUPVAL:
            risa_disassembler_disassemble_upvalue_get_instruction(disassembler, "GUPVAL");
            break;
        case RISA_OP_SUPVAL:
            risa_disassembler_disassemble_upvalue_set_instruction(disassembler, "SUPVAL");
            break;
        case RISA_OP_CUPVAL:
            risa_disassembler_disassemble_upvalue_close_instruction(disassembler, "CUPVAL");
            break;
        case RISA_OP_CLSR:
            risa_disassembler_disassemble_closure_instruction(disassembler, "CLSR");
            break;
        case RISA_OP_ARR:
            risa_disassembler_disassemble_byte_instruction(disassembler, "ARR");
            break;
        case RISA_OP_PARR:
            risa_disassembler_disassemble_array_push_instruction(disassembler, "PARR", types);
            break;
        case RISA_OP_LEN:
            risa_disassembler_disassemble_array_length_instruction(disassembler, "LEN");
            break;
        case RISA_OP_OBJ:
            risa_disassembler_disassemble_byte_instruction(disassembler, "OBJ");
            break;
        case RISA_OP_GET:
            risa_disassembler_disassemble_get_instruction(disassembler, "GET", types);
            break;
        case RISA_OP_SET:
            risa_disassembler_disassemble_set_instruction(disassembler, "SET", types);
            break;
        case RISA_OP_NULL:
            risa_disassembler_disassemble_byte_instruction(disassembler, "NULL");
            break;
        case RISA_OP_TRUE:
            risa_disassembler_disassemble_byte_instruction(disassembler, "TRUE");
            break;
        case RISA_OP_FALSE:
            risa_disassembler_disassemble_byte_instruction(disassembler, "FALSE");
            break;
        case RISA_OP_NOT:
            risa_disassembler_disassemble_unary_instruction(disassembler, "NOT", types);
            break;
        case RISA_OP_BNOT:
            risa_disassembler_disassemble_unary_instruction(disassembler, "BNOT", types);
            break;
        case RISA_OP_NEG:
            risa_disassembler_disassemble_unary_instruction(disassembler, "NEG", types);
            break;
        case RISA_OP_INC:
            risa_disassembler_disassemble_byte_instruction(disassembler, "INC");
            break;
        case RISA_OP_DEC:
            risa_disassembler_disassemble_byte_instruction(disassembler, "DEC");
            break;
        case RISA_OP_ADD:
            risa_disassembler_disassemble_binary_instruction(disassembler, "ADD", types);
            break;
        case RISA_OP_SUB:
            risa_disassembler_disassemble_binary_instruction(disassembler, "SUB", types);
            break;
        case RISA_OP_MUL:
            risa_disassembler_disassemble_binary_instruction(disassembler, "MUL", types);
            break;
        case RISA_OP_DIV:
            risa_disassembler_disassemble_binary_instruction(disassembler, "DIV", types);
            break;
        case RISA_OP_MOD:
            risa_disassembler_disassemble_binary_instruction(disassembler, "MOD", types);
            break;
        case RISA_OP_SHL:
            risa_disassembler_disassemble_binary_instruction(disassembler, "SHL", types);
            break;
        case RISA_OP_SHR:
            risa_disassembler_disassemble_binary_instruction(disassembler, "SHR", types);
            break;
        case RISA_OP_LT:
            risa_disassembler_disassemble_binary_instruction(disassembler, "LT", types);
            break;
        case RISA_OP_LTE:
            risa_disassembler_disassemble_binary_instruction(disassembler, "LTE", types);
            break;
        case RISA_OP_EQ:
            risa_disassembler_disassemble_binary_instruction(disassembler, "EQ", types);
            break;
        case RISA_OP_NEQ:
            risa_disassembler_disassemble_binary_instruction(disassembler, "NEQ", types);
            break;
        case RISA_OP_BAND:
            risa_disassembler_disassemble_binary_instruction(disassembler, "BAND", types);
            break;
        case RISA_OP_BXOR:
            risa_disassembler_disassemble_binary_instruction(disassembler, "BXOR", types);
            break;
        case RISA_OP_BOR:
            risa_disassembler_disassemble_binary_instruction(disassembler, "BOR", types);
            break;
        case RISA_OP_TEST:
            risa_disassembler_disassemble_byte_instruction(disassembler, "TEST");
            break;
        case RISA_OP_NTEST:
            risa_disassembler_disassemble_byte_instruction(disassembler, "NTEST");
            break;
        case RISA_OP_JMP:
            risa_disassembler_disassemble_byte_instruction(disassembler, "JMP");
            break;
        case RISA_OP_JMPW:
            risa_disassembler_disassemble_word_instruction(disassembler, "JMPW");
            break;
        case RISA_OP_BJMP:
            risa_disassembler_disassemble_byte_instruction(disassembler, "BJMP");
            break;
        case RISA_OP_BJMPW:
            risa_disassembler_disassemble_word_instruction(disassembler, "BJMPW");
            break;
        case RISA_OP_CALL:
            risa_disassembler_disassemble_call_instruction(disassembler, "CALL");
            break;
        case RISA_OP_RET:
            risa_disassembler_disassemble_byte_instruction(disassembler, "RET");
            break;
        case RISA_OP_ACC:
            risa_disassembler_disassemble_acc_instruction(disassembler, "ACC", types);
            break;
        case RISA_OP_DIS:
            risa_disassembler_disassemble_byte_instruction(disassembler, "DIS");
            break;
        default:
            RISA_OUT(disassembler->io, "<UNK>");
    }

    RISA_DISASM_OFFSET += RISA_TODLR_INSTRUCTION_SIZE;
}

void risa_disassembler_disassemble_binary_instruction(RisaDisassembler* disassembler, const char *name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c %4d%c\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'),
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3],
             (types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r'));
}

void risa_disassembler_disassemble_unary_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));
}

void risa_disassembler_disassemble_byte_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1]);
}

void risa_disassembler_disassemble_acc_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d%c\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));
}

void risa_disassembler_disassemble_word_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %5hu\n", name,
             (uint16_t) RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1]);
}

void risa_disassembler_disassemble_constant_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    '", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);

    risa_value_print(&disassembler->io, RISA_DISASM_CLUSTER->constants.values[RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]]);

    RISA_OUT(disassembler->io, "'\n");
}

void risa_disassembler_disassemble_mov_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);
}

void risa_disassembler_disassemble_call_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);
}

void risa_disassembler_disassemble_global_define_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c    '", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));

    risa_value_print(&disassembler->io, RISA_DISASM_CLUSTER->constants.values[RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1]]);

    RISA_OUT(disassembler->io, "'\n");
}

void risa_disassembler_disassemble_global_get_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    '", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);

    risa_value_print(&disassembler->io, RISA_DISASM_CLUSTER->constants.values[RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]]);

    RISA_OUT(disassembler->io, "'\n");
}

void risa_disassembler_disassemble_global_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c    '", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));

    risa_value_print(&disassembler->io, RISA_DISASM_CLUSTER->constants.values[RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1]]);

    RISA_OUT(disassembler->io, "'\n");
}

void risa_disassembler_disassemble_upvalue_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d    %s\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2] == 0 ? "upvalue" : "local");
}

void risa_disassembler_disassemble_upvalue_get_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);
}

void risa_disassembler_disassemble_upvalue_set_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);
}

void risa_disassembler_disassemble_upvalue_close_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1]);
}

void risa_disassembler_disassemble_closure_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d %4d", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3]);

    RISA_OUT(disassembler->io, "\n");
}

void risa_disassembler_disassemble_array_push_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             (types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r'));
}

void risa_disassembler_disassemble_array_length_instruction(RisaDisassembler* disassembler, const char* name) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d\n", name, RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1], RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2]);
}

void risa_disassembler_disassemble_get_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    if(types & RISA_TODLR_TYPE_RIGHT_MASK) {
        RISA_OUT(disassembler->io, "%-16s %4d %4d %4d%c '", name,
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3],
                 (types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r'));

        risa_value_print(&disassembler->io, RISA_DISASM_CLUSTER->constants.values[RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3]]);
        RISA_OUT(disassembler->io, "'\n");
    } else {
        RISA_OUT(disassembler->io, "%-16s %4d %4d %4d%c\n", name,
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
                 RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3],
                 (types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r'));
    }
}

void risa_disassembler_disassemble_set_instruction(RisaDisassembler* disassembler, const char* name, uint8_t types) {
    RISA_OUT(disassembler->io, "%-16s %4d %4d%c %4d%c\n", name,
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 1],
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 2],
             types & RISA_TODLR_TYPE_LEFT_MASK ? 'c' : 'r',
             RISA_DISASM_CLUSTER->bytecode[RISA_DISASM_OFFSET + 3],
             types & RISA_TODLR_TYPE_RIGHT_MASK ? 'c' : 'r');
}

#undef RISA_DISASM_OFFSET
#undef RISA_DISASM_CLUSTER