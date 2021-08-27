#include "assembler.h"

#include "../io/log.h"
#include "../lib/charlib.h"

#include <stdlib.h>

static void risa_assembler_assemble_mode_line                 (RisaAssembler*);
static void risa_assembler_assemble_data_mode_switch          (RisaAssembler*);
static void risa_assembler_assemble_code_mode_switch          (RisaAssembler*);
static void risa_assembler_assemble_data_line                 (RisaAssembler*);
static void risa_assembler_assemble_code_line                 (RisaAssembler*);

static void risa_assembler_assemble_byte_data                 (RisaAssembler*);
static void risa_assembler_assemble_int_data                  (RisaAssembler*);
static void risa_assembler_assemble_float_data                (RisaAssembler*);
static void risa_assembler_assemble_string_data               (RisaAssembler*);
static void risa_assembler_assemble_bool_data                 (RisaAssembler*);
static void risa_assembler_assemble_function_data             (RisaAssembler*);

static void risa_assembler_assemble_global   (RisaAssembler*); // Takes a string and a register/constant.
static void risa_assembler_assemble_cnst     (RisaAssembler*); // Takes a register and a constant.
static void risa_assembler_assemble_copy     (RisaAssembler*); // Takes two registers.
static void risa_assembler_assemble_producer (RisaAssembler*); // Takes a register.
static void risa_assembler_assemble_acc      (RisaAssembler*); // Takes either a register or a constant.
static void risa_assembler_assemble_unary    (RisaAssembler*); // Takes a register and a register/constant.
static void risa_assembler_assemble_binary   (RisaAssembler*); // Takes a register and two registers/constants.
static void risa_assembler_assemble_jump     (RisaAssembler*); // Takes a number, either byte or word.
static void risa_assembler_assemble_consumer (RisaAssembler*); // Takes a register or RISA_TODLR_REGISTER_NULL.
static void risa_assembler_assemble_call     (RisaAssembler*); // Takes a register and a number.
static void risa_assembler_assemble_gglob    (RisaAssembler*); // Takes a register and a string.
static void risa_assembler_assemble_upval    (RisaAssembler*); // Takes two numbers and a bool.
static void risa_assembler_assemble_gupval   (RisaAssembler*); // Takes a register and two numbers.
static void risa_assembler_assemble_supval   (RisaAssembler*); // Takes two numbers and a register.
static void risa_assembler_assemble_clsr     (RisaAssembler*); // Takes two registers and a number.
static void risa_assembler_assemble_get      (RisaAssembler*); // Takes two registers and a register/constant.
static void risa_assembler_assemble_set      (RisaAssembler*); // Takes a register and two registers/constants.

static void risa_assembler_emit_byte                          (RisaAssembler*, uint8_t);
static void risa_assembler_emit_word                          (RisaAssembler*, uint16_t);

static uint8_t          risa_assembler_read_reg               (RisaAssembler*);
static uint16_t         risa_assembler_read_const             (RisaAssembler*);
static uint16_t         risa_assembler_read_byte              (RisaAssembler*);
static uint16_t         risa_assembler_read_int               (RisaAssembler*);
static uint16_t         risa_assembler_read_float             (RisaAssembler*);
static uint16_t         risa_assembler_read_string            (RisaAssembler*);
static uint16_t         risa_assembler_read_any_const         (RisaAssembler*);
static uint16_t         risa_assembler_read_identifier        (RisaAssembler*);
static int64_t          risa_assembler_read_number            (RisaAssembler*);
static bool             risa_assembler_read_bool              (RisaAssembler*);

static bool             risa_assembler_identifier_add         (RisaAssembler*, const char*, uint32_t, uint16_t);
static uint32_t         risa_assembler_identifier_resolve     (RisaAssembler*, RisaAsmToken*);

static uint16_t         risa_assembler_create_constant        (RisaAssembler*, RisaValue);
static uint16_t         risa_assembler_create_string_constant (RisaAssembler*, const char*, uint32_t);
static RisaDenseString* risa_assembler_create_string_entry    (RisaAssembler* assembler, const char* start, uint32_t length);

RisaAssembler* risa_assembler_create() {
    RisaAssembler* assembler = RISA_MEM_ALLOC(sizeof(RisaAssembler));

    risa_assembler_init(assembler);

    return assembler;
}

void risa_assembler_init(RisaAssembler* assembler) {
    risa_io_init(&assembler->io);
    assembler->super = NULL;
    assembler->strings = NULL;

    risa_cluster_init(&assembler->cluster);
    risa_map_init(&assembler->identifiers);

    assembler->canSwitchToData = true;
    assembler->mode = RISA_ASM_MODE_CODE;
}

void risa_assembler_delete(RisaAssembler* assembler) {
    for(uint32_t i = 0; i < assembler->identifiers.capacity; ++i) {
        RisaMapEntry* entry = &assembler->identifiers.entries[i];

        if(entry->key != NULL)
            risa_dense_string_delete(entry->key);
    }

    risa_map_delete(&assembler->identifiers);
}

void risa_assembler_free(RisaAssembler* assembler) {
    risa_assembler_delete(assembler);

    RISA_MEM_FREE(assembler);
}

AssemblerStatus risa_assembler_assemble(RisaAssembler* assembler, const char* str, const char* stoppers) {
    RisaAsmParser parser;
    risa_asm_parser_init(&parser);
    risa_io_clone(&parser.io, &assembler->io); // Use the same IO interface for the parser.

    assembler->parser = &parser;

    risa_asm_lexer_init(&assembler->parser->lexer);
    risa_asm_lexer_source(&assembler->parser->lexer, str);

    assembler->parser->lexer.stoppers = stoppers;

    risa_asm_parser_advance(assembler->parser);

    while(assembler->parser->current.type != RISA_ASM_TOKEN_EOF) {
        risa_assembler_assemble_mode_line(assembler);

        if(assembler->canSwitchToData)
            assembler->canSwitchToData = false;

        if(assembler->parser->panic)
            risa_asm_parser_sync(assembler->parser);
    }

    return assembler->parser->error ? RISA_ASM_STATUS_ERROR : RISA_ASM_STATUS_OK;
}

static void risa_assembler_assemble_mode_line(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_DOT) {
        risa_asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == RISA_ASM_TOKEN_DATA)
            risa_assembler_assemble_data_mode_switch(assembler);
        else if(assembler->parser->current.type == RISA_ASM_TOKEN_CODE)
            risa_assembler_assemble_code_mode_switch(assembler);
        else risa_asm_parser_error_at_current(assembler->parser, "Expected 'data' or 'code' after dot");
    } else {
        switch(assembler->mode) {
            case RISA_ASM_MODE_DATA:
                risa_assembler_assemble_data_line(assembler);
                break;
            case RISA_ASM_MODE_CODE:
                risa_assembler_assemble_code_line(assembler);
                break;
        }
    }
}

static void risa_assembler_assemble_data_mode_switch(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(!assembler->canSwitchToData) {
        risa_asm_parser_error_at_current(assembler->parser, "Cannot switch to data mode");
        return;
    } else if(assembler->mode == RISA_ASM_MODE_DATA) {
        risa_asm_parser_error_at_current(assembler->parser, "RisaAssembler is already in data mode");
        return;
    } else assembler->mode = RISA_ASM_MODE_DATA;
}

static void risa_assembler_assemble_code_mode_switch(RisaAssembler* assembler) {
    if(!assembler->canSwitchToData && assembler->mode == RISA_ASM_MODE_CODE) {
        risa_asm_parser_error_at_current(assembler->parser, "RisaAssembler is already in code mode");
        return;
    } else assembler->mode = RISA_ASM_MODE_CODE;

    risa_asm_parser_advance(assembler->parser);
}

static void risa_assembler_assemble_data_line(RisaAssembler* assembler) {
    switch(assembler->parser->current.type) {
        case RISA_ASM_TOKEN_BYTE_TYPE:
            risa_assembler_assemble_byte_data(assembler);
            break;
        case RISA_ASM_TOKEN_INT_TYPE:
            risa_assembler_assemble_int_data(assembler);
            break;
        case RISA_ASM_TOKEN_FLOAT_TYPE:
            risa_assembler_assemble_float_data(assembler);
            break;
        case RISA_ASM_TOKEN_BOOL_TYPE:
            risa_assembler_assemble_bool_data(assembler);
            break;
        case RISA_ASM_TOKEN_STRING_TYPE:
            risa_assembler_assemble_string_data(assembler);
            break;
        case RISA_ASM_TOKEN_FUNCTION_TYPE:
            risa_assembler_assemble_function_data(assembler);
            break;
        default:
            risa_asm_parser_error_at_current(assembler->parser, "Expected data type");
            risa_asm_parser_advance(assembler->parser);
            return;
    }
}

static void risa_assembler_assemble_code_line(RisaAssembler* assembler) {
    switch(assembler->parser->current.type) {
        case RISA_ASM_TOKEN_CNST:
        case RISA_ASM_TOKEN_CNSTW:
            risa_assembler_assemble_cnst(assembler);
            break;
        case RISA_ASM_TOKEN_MOV:
        case RISA_ASM_TOKEN_CLONE:
        case RISA_ASM_TOKEN_LEN:
            risa_assembler_assemble_copy(assembler);
            break;
        case RISA_ASM_TOKEN_DGLOB:
        case RISA_ASM_TOKEN_SGLOB:
            risa_assembler_assemble_global(assembler);
            break;
        case RISA_ASM_TOKEN_GGLOB:
            risa_assembler_assemble_gglob(assembler);
            break;
        case RISA_ASM_TOKEN_UPVAL:
            risa_assembler_assemble_upval(assembler);
            break;
        case RISA_ASM_TOKEN_GUPVAL:
            risa_assembler_assemble_gupval(assembler);
            break;
        case RISA_ASM_TOKEN_SUPVAL:
            risa_assembler_assemble_supval(assembler);
            break;
        case RISA_ASM_TOKEN_CLSR:
            risa_assembler_assemble_clsr(assembler);
            break;
        case RISA_ASM_TOKEN_GET:
            risa_assembler_assemble_get(assembler);
            break;
        case RISA_ASM_TOKEN_SET:
            risa_assembler_assemble_set(assembler);
            break;
        case RISA_ASM_TOKEN_NULL:
        case RISA_ASM_TOKEN_TRUE:
        case RISA_ASM_TOKEN_FALSE:
        case RISA_ASM_TOKEN_ARR:
        case RISA_ASM_TOKEN_OBJ:
        case RISA_ASM_TOKEN_INC:
        case RISA_ASM_TOKEN_DEC:
        case RISA_ASM_TOKEN_TEST:
        case RISA_ASM_TOKEN_NTEST:
        case RISA_ASM_TOKEN_CUPVAL:
            risa_assembler_assemble_producer(assembler);
            break;
        case RISA_ASM_TOKEN_ACC:
            risa_assembler_assemble_acc(assembler);
            break;
        case RISA_ASM_TOKEN_NOT:
        case RISA_ASM_TOKEN_BNOT:
        case RISA_ASM_TOKEN_NEG:
        case RISA_ASM_TOKEN_PARR:
            risa_assembler_assemble_unary(assembler);
            break;
        case RISA_ASM_TOKEN_ADD:
        case RISA_ASM_TOKEN_SUB:
        case RISA_ASM_TOKEN_MUL:
        case RISA_ASM_TOKEN_DIV:
        case RISA_ASM_TOKEN_MOD:
        case RISA_ASM_TOKEN_SHL:
        case RISA_ASM_TOKEN_SHR:
        case RISA_ASM_TOKEN_LT:
        case RISA_ASM_TOKEN_LTE:
        case RISA_ASM_TOKEN_EQ:
        case RISA_ASM_TOKEN_NEQ:
        case RISA_ASM_TOKEN_BAND:
        case RISA_ASM_TOKEN_BXOR:
        case RISA_ASM_TOKEN_BOR:
            risa_assembler_assemble_binary(assembler);
            break;
        case RISA_ASM_TOKEN_JMP:
        case RISA_ASM_TOKEN_JMPW:
        case RISA_ASM_TOKEN_BJMP:
        case RISA_ASM_TOKEN_BJMPW:
            risa_assembler_assemble_jump(assembler);
            break;
        case RISA_ASM_TOKEN_CALL:
            risa_assembler_assemble_call(assembler);
            break;
        case RISA_ASM_TOKEN_RET:
        case RISA_ASM_TOKEN_DIS:
            risa_assembler_assemble_consumer(assembler);
            break;
        default:
            risa_asm_parser_error_at_current(assembler->parser, "Expected instruction");
            risa_asm_parser_advance(assembler->parser);
            return;
    }
}

static void risa_assembler_assemble_byte_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_BYTE) {
        uint16_t index = risa_assembler_read_byte(assembler);
        risa_asm_parser_advance(assembler->parser);

        if(id.type != RISA_ASM_TOKEN_ERROR) {
            if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
                risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
        }
    } else if(assembler->parser->current.type == RISA_ASM_TOKEN_INT) {
        uint16_t index = risa_assembler_read_int(assembler);
        risa_asm_parser_advance(assembler->parser);

        if(index == UINT16_MAX)
            return;
        if(RISA_AS_INT(assembler->cluster.constants.values[index]) > 255) {
            risa_asm_parser_error_at_current(assembler->parser, "Byte value out of range (0-255)");
            return;
        }

        if(id.type != RISA_ASM_TOKEN_ERROR)
            if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
                risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        risa_asm_parser_error_at_current(assembler->parser, "Expected byte");
        return;
    }
}

static void risa_assembler_assemble_int_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_INT) {
        uint16_t index = risa_assembler_read_int(assembler);
        risa_asm_parser_advance(assembler->parser);

        if(id.type != RISA_ASM_TOKEN_ERROR)
            if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
                risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        risa_asm_parser_error_at_current(assembler->parser, "Expected int");
        return;
    }
}

static void risa_assembler_assemble_float_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_FLOAT) {
        uint16_t index = risa_assembler_read_float(assembler);
        risa_asm_parser_advance(assembler->parser);

        if(id.type != RISA_ASM_TOKEN_ERROR)
            if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
                risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        risa_asm_parser_error_at_current(assembler->parser, "Expected float");
        return;
    }
}

static void risa_assembler_assemble_string_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_STRING) {
        uint16_t index = risa_assembler_read_string(assembler);
        risa_asm_parser_advance(assembler->parser);

        if(id.type != RISA_ASM_TOKEN_ERROR)
            if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
                risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        risa_asm_parser_error_at_current(assembler->parser, "Expected string");
        return;
    }
}

static void risa_assembler_assemble_bool_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type != RISA_ASM_TOKEN_TRUE && assembler->parser->current.type != RISA_ASM_TOKEN_FALSE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected bool");
        return;
    }

    uint16_t index = risa_assembler_read_bool(assembler);
    risa_asm_parser_advance(assembler->parser);

    if(id.type != RISA_ASM_TOKEN_ERROR)
        if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
            risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
}

static void risa_assembler_assemble_function_data(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    RisaAsmToken id = { .type = RISA_ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        risa_asm_parser_advance(assembler->parser);
    }

    risa_asm_parser_consume(assembler->parser, RISA_ASM_TOKEN_LEFT_PAREN, "Expected '('");

    int64_t argc = 0;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_INT) {
        if(!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &argc) || argc < 0 || argc > RISA_TODLR_REGISTER_COUNT) {
            risa_asm_parser_error_at_current(assembler->parser, "Argument count out of range (0-250)");
            return;
        }

        risa_asm_parser_advance(assembler->parser);
    }

    risa_asm_parser_consume(assembler->parser, RISA_ASM_TOKEN_RIGHT_PAREN, "Expected ')'");
    risa_asm_parser_consume(assembler->parser, RISA_ASM_TOKEN_LEFT_BRACE, "Expected '{'");

    RisaAssembler iasm;

    risa_assembler_init(&iasm);

    RisaDenseFunction* func = risa_dense_function_create(&func);

    RisaDenseString* funcName;

    if(id.type != RISA_ASM_TOKEN_ERROR)
        funcName = risa_assembler_create_string_entry(assembler, id.start, id.size);
    else funcName = risa_assembler_create_string_entry(assembler, "lambda", 6);

    if(funcName == NULL)
        return;

    func->arity = (uint8_t) argc;
    func->name = funcName;

    iasm.cluster = func->cluster;
    iasm.strings = assembler->strings;

    risa_assembler_assemble(&iasm, assembler->parser->lexer.start, "}");

    func->cluster = iasm.cluster;
    func->cluster.constants = iasm.cluster.constants;

    assembler->parser->lexer.start = iasm.parser->lexer.start;
    assembler->parser->lexer.current = iasm.parser->lexer.current;
    assembler->parser->lexer.index += iasm.parser->lexer.index - 1;

    if(iasm.parser->error)
        assembler->parser->error = true;


    // Edge case when the stopper is '}'
    assembler->parser->lexer.ignoreStoppers = true;

    risa_asm_parser_advance(assembler->parser);

    assembler->parser->lexer.ignoreStoppers = false;

    risa_assembler_delete(&iasm);

    risa_asm_parser_consume(assembler->parser, RISA_ASM_TOKEN_RIGHT_BRACE, "Expected '}'");

    uint16_t index = risa_assembler_create_constant(assembler, RISA_DENSE_VALUE(func));

    if(id.type != RISA_ASM_TOKEN_ERROR)
        if(!risa_assembler_identifier_add(assembler, id.start, id.size, index))
            risa_asm_parser_error_at_current(assembler->parser, "Identifier already exists");
}

static void risa_assembler_assemble_global(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    uint16_t dest = risa_assembler_read_string(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(dest > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
        return;
    }

    uint16_t left;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        left = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_assembler_emit_byte(assembler, op);
    } else {
        left = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_cnst(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);
    bool isWord = (op == RISA_OP_CNSTW);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t left = risa_assembler_read_any_const(assembler);

    if(isWord) {
        if(left > UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-65535)");
            return;
        }
    } else {
        if(left > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255); consider using 'CNSTW'");
            return;
        }
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, op);
    risa_assembler_emit_byte(assembler, (uint8_t) dest);

    if(isWord) {
        risa_assembler_emit_word(assembler, left);
    } else {
        risa_assembler_emit_byte(assembler, (uint8_t) left);
        risa_assembler_emit_byte(assembler, 0);
    }
}

static void risa_assembler_assemble_copy(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, op);
    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_producer(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, op);
    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, 0);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_acc(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    uint8_t dest;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        dest = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_assembler_emit_byte(assembler, op);
    } else {
        dest = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(dest > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, 0);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_unary(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t left;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        left = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_assembler_emit_byte(assembler, op);
    } else {
        left = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_binary(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        left = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
            right = risa_assembler_read_reg(assembler);

            if(assembler->parser->panic)
                return;

            risa_assembler_emit_byte(assembler, op);
        } else {
            right = risa_assembler_read_any_const(assembler);

            if(assembler->parser->panic)
                return;

            if(right > UINT8_MAX) {
                risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
                return;
            }

            risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_RIGHT_MASK);
        }
    } else {
        left = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        risa_asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
            right = risa_assembler_read_reg(assembler);

            if(assembler->parser->panic)
                return;

            risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
        } else {
            right = risa_assembler_read_any_const(assembler);

            if(assembler->parser->panic)
                return;

            if(right > UINT8_MAX) {
                risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
                return;
            }

            risa_assembler_emit_byte(assembler, op | RISA_TODLR_TYPE_MASK);
        }
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, (uint8_t) right);
}

static void risa_assembler_assemble_jump(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);
    bool isWord = (op == RISA_OP_JMPW || op == RISA_OP_BJMPW); // Word variation: 0-65535 instead of 0-255

    risa_asm_parser_advance(assembler->parser);

    int64_t dest = risa_assembler_read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(isWord) {
        if(dest > UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-65535)");
            return;
        }
    } else {
        if(dest > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
            return;
        }
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, op);

    if(isWord) {
        risa_assembler_emit_word(assembler, (uint16_t) dest);
    } else {
        risa_assembler_emit_byte(assembler, (uint8_t) dest);
        risa_assembler_emit_byte(assembler, 0);
    }

    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_consumer(RisaAssembler* assembler) {
    uint8_t op = risa_asm_token_to_opcode(assembler->parser->current.type);

    risa_asm_parser_advance(assembler->parser);

    int64_t dest;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        dest = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_asm_parser_advance(assembler->parser);
    } else {
        if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
            risa_asm_parser_error_at_current(assembler->parser, "Expected register or number '" RISA_TODLR_REGISTER_NULL_STR "'");
            return;
        }

        dest = risa_assembler_read_number(assembler);

        if(assembler->parser->panic)
            return;

        if(dest != RISA_TODLR_REGISTER_NULL) {
            risa_asm_parser_error_at_current(assembler->parser, "Unexpected number value; must be " RISA_TODLR_REGISTER_NULL_STR);
            return;
        }

        risa_asm_parser_advance(assembler->parser);
    }

    risa_assembler_emit_byte(assembler, op);
    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, 0);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_call(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = risa_assembler_read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_CALL);
    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_gglob(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t left = risa_assembler_read_string(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_GGLOB);
    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_upval(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t dest = risa_assembler_read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(dest > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_TRUE && assembler->parser->current.type != RISA_ASM_TOKEN_FALSE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'bool'");
        return;
    }

    bool left = risa_assembler_read_bool(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_UPVAL);
    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_gupval(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = risa_assembler_read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_GUPVAL);
    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_supval(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t dest = risa_assembler_read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(dest > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_SUPVAL);
    risa_assembler_emit_byte(assembler, (uint8_t) dest);
    risa_assembler_emit_byte(assembler, left);
    risa_assembler_emit_byte(assembler, 0);
}

static void risa_assembler_assemble_clsr(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_INT && assembler->parser->current.type != RISA_ASM_TOKEN_BYTE) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t right = risa_assembler_read_number(assembler);

    if(right > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, RISA_OP_CLSR);
    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, left);
    risa_assembler_emit_byte(assembler, (uint8_t) right);
}

static void risa_assembler_assemble_get(RisaAssembler* assembler) {
    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t right;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        right = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        risa_assembler_emit_byte(assembler, RISA_OP_GET);
    } else {
        right = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(right > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        risa_assembler_emit_byte(assembler, RISA_OP_GET | RISA_TODLR_TYPE_RIGHT_MASK);
    }

    risa_asm_parser_advance(assembler->parser);

    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, left);
    risa_assembler_emit_byte(assembler, (uint8_t) right);
}

static void risa_assembler_assemble_set(RisaAssembler* assembler) {
    bool isLeftConst;

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != RISA_ASM_TOKEN_REGISTER) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = risa_assembler_read_reg(assembler);

    if(assembler->parser->panic)
        return;

    risa_asm_parser_advance(assembler->parser);

    uint16_t left;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        left = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        isLeftConst = false;
    } else {
        left = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        isLeftConst = true;
    }

    risa_asm_parser_advance(assembler->parser);

    uint16_t right;

    if(assembler->parser->current.type == RISA_ASM_TOKEN_REGISTER) {
        right = risa_assembler_read_reg(assembler);

        if(assembler->parser->panic)
            return;

        if(isLeftConst)
            risa_assembler_emit_byte(assembler, RISA_OP_SET | RISA_TODLR_TYPE_LEFT_MASK);
        else risa_assembler_emit_byte(assembler, RISA_OP_SET);
    } else {
        right = risa_assembler_read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(right > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        if(isLeftConst)
            risa_assembler_emit_byte(assembler, RISA_OP_SET | RISA_TODLR_TYPE_MASK);
        else risa_assembler_emit_byte(assembler, RISA_OP_SET | RISA_TODLR_TYPE_RIGHT_MASK);
    }

    risa_asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    risa_assembler_emit_byte(assembler, dest);
    risa_assembler_emit_byte(assembler, (uint8_t) left);
    risa_assembler_emit_byte(assembler, (uint8_t) right);
}

static void risa_assembler_emit_byte(RisaAssembler* assembler, uint8_t byte) {
    risa_cluster_write(&assembler->cluster, byte, 0);
}

static void risa_assembler_emit_word(RisaAssembler* assembler, uint16_t word) {
    risa_assembler_emit_byte(assembler, ((uint8_t *) &word)[0]);
    risa_assembler_emit_byte(assembler, ((uint8_t *) &word)[1]);
}

static uint8_t risa_assembler_read_reg(RisaAssembler* assembler) {
    int64_t num;

    if (!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num) || num > (RISA_TODLR_REGISTER_COUNT - 1)) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is not a valid register (0-249)");
        return RISA_TODLR_REGISTER_NULL;
    }

    return (uint8_t) num;
}

static uint16_t risa_assembler_read_const(RisaAssembler* assembler) {
    int64_t num;

    if(!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num) || num > UINT16_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is not a valid constant (0-65535)");
        return UINT16_MAX;
    }

    return (uint16_t) num;
}

static uint16_t risa_assembler_read_byte(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_read_identifier(assembler);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];

            if(value->type != RISA_VAL_BYTE) {
                risa_asm_parser_error_at_current(assembler->parser, "Expected byte");
                return UINT16_MAX;
            }

            return index;
        }
    }

    int64_t num;

    if(!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num) || num > UINT8_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is invalid for type 'byte'");
        return UINT16_MAX;
    }

    return risa_assembler_create_constant(assembler, RISA_BYTE_VALUE((uint8_t) num));
}

static uint16_t risa_assembler_read_int(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_read_identifier(assembler);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];

            if(value->type != RISA_VAL_INT) {
                risa_asm_parser_error_at_current(assembler->parser, "Expected int");
                return UINT16_MAX;
            }

            return index;
        }
    }

    int64_t num;

    if(!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num)) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is invalid for type 'int'");
        return UINT16_MAX;
    }

    return risa_assembler_create_constant(assembler, RISA_INT_VALUE(num));
}

static uint16_t risa_assembler_read_float(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_read_identifier(assembler);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];

            if(value->type != RISA_VAL_FLOAT) {
                risa_asm_parser_error_at_current(assembler->parser, "Expected float");
                return UINT16_MAX;
            }

            return index;
        }
    }

    double num;

    if(!risa_lib_charlib_strntod(assembler->parser->current.start, assembler->parser->current.size, &num)) {
        risa_asm_parser_error_at_current(assembler->parser, "Number is invalid for type 'float'");
        return UINT16_MAX;
    }

    return risa_assembler_create_constant(assembler, RISA_FLOAT_VALUE(num));
}

static uint16_t risa_assembler_read_string(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_read_identifier(assembler);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];

            if(!risa_value_is_dense_of_type(*value, RISA_DVAL_STRING)) {
                risa_asm_parser_error_at_current(assembler->parser, "Expected string");
                return UINT16_MAX;
            }

            return index;
        }
    }

    if(assembler->parser->current.type != RISA_ASM_TOKEN_STRING) {
        risa_asm_parser_error_at_current(assembler->parser, "Expected string");
        return UINT16_MAX;
    }

    const char* start = assembler->parser->current.start + 1;
    uint32_t length = assembler->parser->current.size - 2;

    const char* ptr = start;
    const char* end = start + length;

    uint32_t escapeCount = 0;

    while(ptr < end)
        if(*(ptr++) == '\\')
            ++escapeCount;

    char* str = (char*) RISA_MEM_ALLOC(length + 1 - escapeCount);
    uint32_t index = 0;

    for(uint32_t i = 0; i < length; ++i) {
        if(start[i] == '\\') {
            if(i < length) {
                switch(start[i + 1]) {
                    case 'a':
                        str[index++] = '\a';
                        break;
                    case 'b':
                        str[index++] = '\b';
                        break;
                    case 'f':
                        str[index++] = '\f';
                        break;
                    case 'n':
                        str[index++] = '\n';
                        break;
                    case 'r':
                        str[index++] = '\r';
                        break;
                    case 't':
                        str[index++] = '\t';
                        break;
                    case 'v':
                        str[index++] = '\v';
                        break;
                    case '\\':
                        str[index++] = '\\';
                        break;
                    case '\'':
                        str[index++] = '\'';
                        break;
                    case '\"':
                        str[index++] = '\"';
                        break;
                    default:
                        RISA_WARNING(assembler->io, "Invalid escape sequence at index %d", assembler->parser->current.index + 1 + i);
                        break;
                }
                ++i;
            }
        } else str[index++] = start[i];
    }

    str[index] = '\0';

    start = str;
    length = index;
    uint32_t hash = risa_map_hash(start, length);

    RisaAssembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(super->strings, interned, RISA_NULL_VALUE);
    }

    RISA_MEM_FREE(str);

    return risa_assembler_create_constant(assembler, RISA_DENSE_VALUE(interned));
}

static uint16_t risa_assembler_read_identifier(RisaAssembler* assembler) {
    uint32_t index = risa_assembler_identifier_resolve(assembler, &assembler->parser->current);

    if(index > UINT16_MAX) {
        risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
        return UINT16_MAX;
    }

    return (uint16_t) index;
}

static uint16_t risa_assembler_read_any_const(RisaAssembler* assembler) {
    switch(assembler->parser->current.type) {
        case RISA_ASM_TOKEN_CONSTANT:
            return risa_assembler_read_const(assembler);
        case RISA_ASM_TOKEN_BYTE:
            return risa_assembler_read_byte(assembler);
        case RISA_ASM_TOKEN_INT:
            return risa_assembler_read_int(assembler);
        case RISA_ASM_TOKEN_FLOAT:
            return risa_assembler_read_float(assembler);
        case RISA_ASM_TOKEN_STRING:
            return risa_assembler_read_string(assembler);
        case RISA_ASM_TOKEN_IDENTIFIER:
            return risa_assembler_read_identifier(assembler);
        default:
            risa_asm_parser_error_at_current(assembler->parser, "Expected constant");
            return UINT16_MAX;
    }
}

static int64_t risa_assembler_read_number(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_identifier_resolve(assembler, &assembler->parser->current);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return INT64_MAX;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];

            if(value->type == RISA_VAL_INT)
                return RISA_AS_INT(*value);
            return RISA_AS_BYTE(*value);
        }
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_INT) {
        int64_t num;

        if (!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num)) {
            risa_asm_parser_error_at_current(assembler->parser, "Number is invalid for type 'int'");
            return INT64_MAX;
        }

        return num;
    }

    if(assembler->parser->current.type == RISA_ASM_TOKEN_BYTE){
        int64_t num;

        if(!risa_lib_charlib_strntoll(assembler->parser->current.start, assembler->parser->current.size, 10, &num) || num > UINT8_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Number is invalid for type 'byte'");
            return INT64_MAX;
        }

        return num;
    }

    risa_asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
    return INT64_MAX;
}

static bool risa_assembler_read_bool(RisaAssembler* assembler) {
    if(assembler->parser->current.type == RISA_ASM_TOKEN_IDENTIFIER) {
        uint16_t index = risa_assembler_identifier_resolve(assembler, &assembler->parser->current);

        if(index == UINT16_MAX) {
            risa_asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return false;
        } else {
            RisaValue* value = &assembler->cluster.constants.values[index];
            return RISA_AS_BOOL(*value);
        }
    }

    return assembler->parser->current.type == RISA_ASM_TOKEN_TRUE;
}

static bool risa_assembler_identifier_add(RisaAssembler* assembler, const char* start, uint32_t length, uint16_t index) {
    if(risa_map_find(&assembler->identifiers, start, length, risa_map_hash(start, length)) == NULL) {
        risa_map_set(&assembler->identifiers, risa_dense_string_from(start, length), RISA_INT_VALUE(index));
        return true;
    } else return false;
}

// uint32_t max if the identifier does not exist.
static uint32_t risa_assembler_identifier_resolve(RisaAssembler* assembler, RisaAsmToken* token) {
    RisaMapEntry* entry = risa_map_find_entry(&assembler->identifiers, token->start, token->size, risa_map_hash(token->start, token->size));

    return entry == NULL ? UINT32_MAX : RISA_AS_INT(entry->value);
}

static uint16_t risa_assembler_create_constant(RisaAssembler* assembler, RisaValue value) {
    size_t index = risa_cluster_write_constant(&assembler->cluster, value);

    if(index > UINT16_MAX) {
        risa_asm_parser_error_at_previous(assembler->parser, "Constant limit exceeded (65535)");
        return -1;
    }

    return (uint16_t) index;
}

static uint16_t risa_assembler_create_string_constant(RisaAssembler* assembler, const char* start, uint32_t length) {
    uint32_t hash = risa_map_hash(start, length);

    RisaAssembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(super->strings, interned, RISA_NULL_VALUE);
    }

    return risa_assembler_create_constant(assembler, RISA_DENSE_VALUE(interned));
}

static RisaDenseString* risa_assembler_create_string_entry(RisaAssembler* assembler, const char* start, uint32_t length) {
    uint32_t hash = risa_map_hash(start, length);

    RisaAssembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(super->strings, interned, RISA_NULL_VALUE);
    }

    return interned;
}