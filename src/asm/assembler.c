#include "assembler.h"

#include "lexer.h"
#include "parser.h"

#include "../value/dense.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void assemble_mode_line(Assembler*);
static void assemble_data_mode_switch(Assembler*);
static void assemble_code_mode_switch(Assembler*);
static void assemble_data_line(Assembler*);
static void assemble_code_line(Assembler*);

static void assemble_byte_data(Assembler*);
static void assemble_int_data(Assembler*);
static void assemble_float_data(Assembler*);
static void assemble_string_data(Assembler*);
static void assemble_bool_data(Assembler*);
static void assemble_function_data(Assembler*);

static void assemble_cnst(Assembler*);
static void assemble_cnstw(Assembler*);
static void assemble_mov(Assembler*);
static void assemble_clone(Assembler*);
static void assemble_dglob(Assembler*);
static void assemble_gglob(Assembler*);
static void assemble_sglob(Assembler*);
static void assemble_upval(Assembler*);
static void assemble_gupval(Assembler*);
static void assemble_supval(Assembler*);
static void assemble_cupval(Assembler*);
static void assemble_clsr(Assembler*);
static void assemble_arr(Assembler*);
static void assemble_parr(Assembler*);
static void assemble_len(Assembler*);
static void assemble_obj(Assembler*);
static void assemble_get(Assembler*);
static void assemble_set(Assembler*);
static void assemble_null(Assembler*);
static void assemble_true(Assembler*);
static void assemble_false(Assembler*);
static void assemble_not(Assembler*);
static void assemble_bnot(Assembler*);
static void assemble_neg(Assembler*);
static void assemble_inc(Assembler*);
static void assemble_dec(Assembler*);
static void assemble_add(Assembler*);
static void assemble_sub(Assembler*);
static void assemble_mul(Assembler*);
static void assemble_div(Assembler*);
static void assemble_mod(Assembler*);
static void assemble_shl(Assembler*);
static void assemble_shr(Assembler*);
static void assemble_lt(Assembler*);
static void assemble_lte(Assembler*);
static void assemble_eq(Assembler*);
static void assemble_neq(Assembler*);
static void assemble_band(Assembler*);
static void assemble_bxor(Assembler*);
static void assemble_bor(Assembler*);
static void assemble_test(Assembler*);
static void assemble_ntest(Assembler*);
static void assemble_jmp(Assembler*);
static void assemble_jmpw(Assembler*);
static void assemble_bjmp(Assembler*);
static void assemble_bjmpw(Assembler*);
static void assemble_call(Assembler*);
static void assemble_ret(Assembler*);
static void assemble_dis(Assembler*);

static void emit_byte(Assembler*, uint8_t);
static void emit_word(Assembler*, uint16_t);

static uint8_t  read_reg(Assembler*);
static uint16_t read_const(Assembler*);
static uint16_t read_byte(Assembler*);
static uint16_t read_int(Assembler*);
static uint16_t read_float(Assembler*);
static uint16_t read_string(Assembler*);
static uint16_t read_any_const(Assembler*);
static uint16_t read_identifier(Assembler*);
static int64_t  read_number(Assembler*);
static bool     read_bool(Assembler*);

static bool     identifier_add(Assembler*, const char*, uint32_t, uint16_t);
static uint32_t identifier_resolve(Assembler*, AsmToken*);

static uint16_t     create_constant(Assembler*, Value);
static uint16_t     create_string_constant(Assembler*, const char*, uint32_t);
static DenseString* create_string_entry(Assembler* assembler, const char* start, uint32_t length);

void assembler_init(Assembler* assembler) {
    assembler->super = NULL;
    assembler->strings = NULL;

    chunk_init(&assembler->chunk);
    map_init(&assembler->identifiers);

    assembler->canSwitchToData = true;
    assembler->mode = ASM_CODE;
}

void assembler_delete(Assembler* assembler) {
    for(uint32_t i = 0; i < assembler->identifiers.capacity; ++i) {
        Entry* entry = &assembler->identifiers.entries[i];

        if(entry->key != NULL)
            dense_string_delete(entry->key);
    }

    map_delete(&assembler->identifiers);
}

AssemblerStatus assembler_assemble(Assembler* assembler, const char* str, const char* stoppers) {
    AsmParser parser;
    asm_parser_init(&parser);

    assembler->parser = &parser;

    asm_lexer_init(&assembler->parser->lexer);
    asm_lexer_source(&assembler->parser->lexer, str);

    assembler->parser->lexer.stoppers = stoppers;

    asm_parser_advance(assembler->parser);

    while(assembler->parser->current.type != ASM_TOKEN_EOF) {
        assemble_mode_line(assembler);

        if(assembler->canSwitchToData)
            assembler->canSwitchToData = false;

        if(assembler->parser->panic)
            asm_parser_sync(assembler->parser);
    }

    return assembler->parser->error ? ASSEMBLER_ERROR : ASSEMBLER_OK;
}

static void assemble_mode_line(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_DOT) {
        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_DATA)
            assemble_data_mode_switch(assembler);
        else if(assembler->parser->current.type == ASM_TOKEN_CODE)
            assemble_code_mode_switch(assembler);
        else asm_parser_error_at_current(assembler->parser, "Expected 'data' or 'code' after dot");
    } else {
        switch(assembler->mode) {
            case ASM_DATA:
                assemble_data_line(assembler);
                break;
            case ASM_CODE:
                assemble_code_line(assembler);
                break;
        }
    }
}

static void assemble_data_mode_switch(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(!assembler->canSwitchToData) {
        asm_parser_error_at_current(assembler->parser, "Cannot switch to data mode");
        return;
    } else if(assembler->mode == ASM_DATA) {
        asm_parser_error_at_current(assembler->parser, "Assembler is already in data mode");
        return;
    } else assembler->mode = ASM_DATA;
}

static void assemble_code_mode_switch(Assembler* assembler) {
    if(!assembler->canSwitchToData && assembler->mode == ASM_CODE) {
        asm_parser_error_at_current(assembler->parser, "Assembler is already in code mode");
        return;
    } else assembler->mode = ASM_CODE;

    asm_parser_advance(assembler->parser);
}

static void assemble_data_line(Assembler* assembler) {
    switch(assembler->parser->current.type) {
        case ASM_TOKEN_BYTE_TYPE:
            assemble_byte_data(assembler);
            break;
        case ASM_TOKEN_INT_TYPE:
            assemble_int_data(assembler);
            break;
        case ASM_TOKEN_FLOAT_TYPE:
            assemble_float_data(assembler);
            break;
        case ASM_TOKEN_BOOL_TYPE:
            assemble_bool_data(assembler);
            break;
        case ASM_TOKEN_STRING_TYPE:
            assemble_string_data(assembler);
            break;
        case ASM_TOKEN_FUNCTION_TYPE:
            assemble_function_data(assembler);
            break;
        default:
            asm_parser_error_at_current(assembler->parser, "Expected data type");
            asm_parser_advance(assembler->parser);
            return;
    }
}

static void assemble_code_line(Assembler* assembler) {
    switch(assembler->parser->current.type) {
        case ASM_TOKEN_CNST:
            assemble_cnst(assembler);
            break;
        case ASM_TOKEN_CNSTW:
            assemble_cnstw(assembler);
            break;
        case ASM_TOKEN_MOV:
            assemble_mov(assembler);
            break;
        case ASM_TOKEN_CLONE:
            assemble_clone(assembler);
            break;
        case ASM_TOKEN_DGLOB:
            assemble_dglob(assembler);
            break;
        case ASM_TOKEN_GGLOB:
            assemble_gglob(assembler);
            break;
        case ASM_TOKEN_SGLOB:
            assemble_sglob(assembler);
            break;
        case ASM_TOKEN_UPVAL:
            assemble_upval(assembler);
            break;
        case ASM_TOKEN_GUPVAL:
            assemble_gupval(assembler);
            break;
        case ASM_TOKEN_SUPVAL:
            assemble_supval(assembler);
            break;
        case ASM_TOKEN_CUPVAL:
            assemble_cupval(assembler);
            break;
        case ASM_TOKEN_CLSR:
            assemble_clsr(assembler);
            break;
        case ASM_TOKEN_ARR:
            assemble_arr(assembler);
            break;
        case ASM_TOKEN_PARR:
            assemble_parr(assembler);
            break;
        case ASM_TOKEN_LEN:
            assemble_len(assembler);
            break;
        case ASM_TOKEN_OBJ:
            assemble_obj(assembler);
            break;
        case ASM_TOKEN_GET:
            assemble_get(assembler);
            break;
        case ASM_TOKEN_SET:
            assemble_set(assembler);
            break;
        case ASM_TOKEN_NULL:
            assemble_null(assembler);
            break;
        case ASM_TOKEN_TRUE:
            assemble_true(assembler);
            break;
        case ASM_TOKEN_FALSE:
            assemble_false(assembler);
            break;
        case ASM_TOKEN_NOT:
            assemble_not(assembler);
            break;
        case ASM_TOKEN_BNOT:
            assemble_bnot(assembler);
            break;
        case ASM_TOKEN_NEG:
            assemble_neg(assembler);
            break;
        case ASM_TOKEN_INC:
            assemble_inc(assembler);
            break;
        case ASM_TOKEN_DEC:
            assemble_dec(assembler);
            break;
        case ASM_TOKEN_ADD:
            assemble_add(assembler);
            break;
        case ASM_TOKEN_SUB:
            assemble_sub(assembler);
            break;
        case ASM_TOKEN_MUL:
            assemble_mul(assembler);
            break;
        case ASM_TOKEN_DIV:
            assemble_div(assembler);
            break;
        case ASM_TOKEN_MOD:
            assemble_mod(assembler);
            break;
        case ASM_TOKEN_SHL:
            assemble_shl(assembler);
            break;
        case ASM_TOKEN_SHR:
            assemble_shr(assembler);
            break;
        case ASM_TOKEN_LT:
            assemble_lt(assembler);
            break;
        case ASM_TOKEN_LTE:
            assemble_lte(assembler);
            break;
        case ASM_TOKEN_EQ:
            assemble_eq(assembler);
            break;
        case ASM_TOKEN_NEQ:
            assemble_neq(assembler);
            break;
        case ASM_TOKEN_BAND:
            assemble_band(assembler);
            break;
        case ASM_TOKEN_BXOR:
            assemble_bxor(assembler);
            break;
        case ASM_TOKEN_BOR:
            assemble_bor(assembler);
            break;
        case ASM_TOKEN_TEST:
            assemble_test(assembler);
            break;
        case ASM_TOKEN_NTEST:
            assemble_ntest(assembler);
            break;
        case ASM_TOKEN_JMP:
            assemble_jmp(assembler);
            break;
        case ASM_TOKEN_JMPW:
            assemble_jmpw(assembler);
            break;
        case ASM_TOKEN_BJMP:
            assemble_bjmp(assembler);
            break;
        case ASM_TOKEN_BJMPW:
            assemble_bjmpw(assembler);
            break;
        case ASM_TOKEN_CALL:
            assemble_call(assembler);
            break;
        case ASM_TOKEN_RET:
            assemble_ret(assembler);
            break;
        default:
            asm_parser_error_at_current(assembler->parser, "Expected instruction");
            asm_parser_advance(assembler->parser);
            return;
    }
}

static void assemble_byte_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == ASM_TOKEN_BYTE) {
        uint16_t index = read_byte(assembler);
        asm_parser_advance(assembler->parser);

        if(id.type != ASM_TOKEN_ERROR) {
            if(!identifier_add(assembler, id.start, id.size, index))
                asm_parser_error_at_current(assembler->parser, "Identifier already exists");
        }
    } else if(assembler->parser->current.type == ASM_TOKEN_INT) {
        uint16_t index = read_int(assembler);
        asm_parser_advance(assembler->parser);

        if(index == (uint16_t) -1u)
            return;
        if(AS_INT(assembler->chunk.constants.values[index]) > 255) {
            asm_parser_error_at_current(assembler->parser, "Byte value out of range (0-255)");
            return;
        }

        if(id.type != ASM_TOKEN_ERROR)
            if(!identifier_add(assembler, id.start, id.size, index))
                asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        asm_parser_error_at_current(assembler->parser, "Expected byte");
        return;
    }
}

static void assemble_int_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == ASM_TOKEN_INT) {
        uint16_t index = read_int(assembler);
        asm_parser_advance(assembler->parser);

        if(id.type != ASM_TOKEN_ERROR)
            if(!identifier_add(assembler, id.start, id.size, index))
                asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        asm_parser_error_at_current(assembler->parser, "Expected int");
        return;
    }
}

static void assemble_float_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == ASM_TOKEN_FLOAT) {
        uint16_t index = read_float(assembler);
        asm_parser_advance(assembler->parser);

        if(id.type != ASM_TOKEN_ERROR)
            if(!identifier_add(assembler, id.start, id.size, index))
                asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        asm_parser_error_at_current(assembler->parser, "Expected float");
        return;
    }
}

static void assemble_string_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type == ASM_TOKEN_STRING) {
        uint16_t index = read_string(assembler);
        asm_parser_advance(assembler->parser);

        if(id.type != ASM_TOKEN_ERROR)
            if(!identifier_add(assembler, id.start, id.size, index))
                asm_parser_error_at_current(assembler->parser, "Identifier already exists");
    } else {
        asm_parser_error_at_current(assembler->parser, "Expected string");
        return;
    }
}

static void assemble_bool_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    if(assembler->parser->current.type != ASM_TOKEN_TRUE && assembler->parser->current.type != ASM_TOKEN_FALSE) {
        asm_parser_error_at_current(assembler->parser, "Expected bool");
        return;
    }

    uint16_t index = read_bool(assembler);
    asm_parser_advance(assembler->parser);

    if(id.type != ASM_TOKEN_ERROR)
        if(!identifier_add(assembler, id.start, id.size, index))
            asm_parser_error_at_current(assembler->parser, "Identifier already exists");
}

static void assemble_function_data(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    AsmToken id = { .type = ASM_TOKEN_ERROR };

    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        id = assembler->parser->current;
        asm_parser_advance(assembler->parser);
    }

    asm_parser_consume(assembler->parser, ASM_TOKEN_LEFT_PAREN, "Expected '('");

    int64_t argc = 0;

    if(assembler->parser->current.type == ASM_TOKEN_INT) {
        argc = strtol(assembler->parser->current.start, NULL, 10);

        if(errno == ERANGE || (argc < 0 || argc > 250)) {
            asm_parser_error_at_current(assembler->parser, "Argument count out of range (0-250)");
            return;
        }

        asm_parser_advance(assembler->parser);
    }

    asm_parser_consume(assembler->parser, ASM_TOKEN_RIGHT_PAREN, "Expected ')'");
    asm_parser_consume(assembler->parser, ASM_TOKEN_LEFT_BRACE, "Expected '{'");

    Assembler iasm;

    assembler_init(&iasm);

    DenseFunction* func = dense_function_create(&func);

    DenseString* funcName;

    if(id.type != ASM_TOKEN_ERROR)
        funcName = create_string_entry(assembler, id.start, id.size);
    else funcName = create_string_entry(assembler, "lambda", 6);

    if(funcName == NULL)
        return;

    func->arity = (uint8_t) argc;
    func->name = funcName;

    iasm.chunk = func->chunk;
    iasm.strings = assembler->strings;

    assembler_assemble(&iasm, assembler->parser->lexer.start, "}");

    func->chunk = iasm.chunk;
    func->chunk.constants = iasm.chunk.constants;

    assembler->parser->lexer.start = iasm.parser->lexer.start;
    assembler->parser->lexer.current = iasm.parser->lexer.current;
    assembler->parser->lexer.index += iasm.parser->lexer.index - 1;

    if(iasm.parser->error)
        assembler->parser->error = true;


    // Edge case when the stopper is '}'
    assembler->parser->lexer.ignoreStoppers = true;

    asm_parser_advance(assembler->parser);

    assembler->parser->lexer.ignoreStoppers = false;

    assembler_delete(&iasm);

    asm_parser_consume(assembler->parser, ASM_TOKEN_RIGHT_BRACE, "Expected '}'");

    uint16_t index = create_constant(assembler, DENSE_VALUE(func));

    if(id.type != ASM_TOKEN_ERROR)
        if(!identifier_add(assembler, id.start, id.size, index))
            asm_parser_error_at_current(assembler->parser, "Identifier already exists");
}

static void assemble_cnst(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left = read_any_const(assembler);

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large; consider using 'CNSTW'");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_CNST);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_cnstw(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t lr = read_any_const(assembler);

    if(lr > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_CNSTW);
    emit_byte(assembler, (uint8_t) dest);
    emit_word(assembler, lr);
}

static void assemble_mov(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    emit_byte(assembler, OP_MOV);
    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, 0);
}

static void assemble_clone(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    emit_byte(assembler, OP_CLONE);
    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, 0);
}

static void assemble_dglob(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    uint16_t dest = read_string(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large");
        return;
    }

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_DGLOB);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_DGLOB | 0x40);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_gglob(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left = read_string(assembler);

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_GGLOB);
    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, 0);
}

static void assemble_sglob(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    uint16_t dest = read_string(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large");
        return;
    }

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_SGLOB);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_SGLOB | 0x40);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_upval(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t dest = read_number(assembler);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_TRUE && assembler->parser->current.type != ASM_TOKEN_FALSE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'bool'");
        return;
    }

    bool left = read_bool(assembler);

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_UPVAL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_gupval(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = read_number(assembler);

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_GUPVAL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_supval(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t dest = read_number(assembler);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    emit_byte(assembler, OP_SUPVAL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_cupval(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_CUPVAL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_clsr(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t right = read_number(assembler);

    if(right > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_CLSR);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_arr(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_ARR);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_parr(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_PARR);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_PARR | 0x40);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_len(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    emit_byte(assembler, OP_LEN);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_obj(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_OBJ);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_get(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(left > 249)
        return;

    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        right = read_reg(assembler);

        if(right > 249)
            return;

        emit_byte(assembler, OP_GET);
    } else {
        right = read_any_const(assembler);

        if(right > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_GET | 0x80);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_set(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_SET);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_SET | 0x40);
    }

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t right = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(right > 249)
        return;

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_null(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_NULL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_true(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_TRUE);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_false(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_FALSE);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_not(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_NOT);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_NOT | 0x80);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_bnot(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_BNOT);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_BNOT | 0x80);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_neg(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        emit_byte(assembler, OP_NEG);
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        emit_byte(assembler, OP_NEG | 0x80);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_inc(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_INC);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_dec(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_DEC);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_add(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_ADD);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_ADD | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_ADD | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_ADD | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_sub(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SUB);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SUB | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SUB | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SUB | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_mul(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_MUL);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_MUL | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_MUL | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_MUL | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_div(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_DIV);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_DIV | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_DIV | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_DIV | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_mod(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_MOD);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_MOD | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_MOD | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_MOD | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_shl(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SHL);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SHL | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SHL | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SHL | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_shr(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SHR);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SHR | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_SHR | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_SHR | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_lt(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_LT);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_LT | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_LT | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_LT | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_lte(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_LTE);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_LTE | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_LTE | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_LTE | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_eq(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_EQ);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_EQ | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_EQ | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_EQ | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_neq(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_NEQ);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_NEQ | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_NEQ | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_NEQ | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_band(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BAND);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BAND | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BAND | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BAND | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_bxor(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BXOR);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BXOR | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BXOR | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BXOR | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_bor(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(left > 249)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BOR);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BOR | 0x40);
        }
    } else {
        left = read_any_const(assembler);

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(right > 249)
                return;

            emit_byte(assembler, OP_BOR | 0x80);
        } else {
            right = read_any_const(assembler);

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large");
                return;
            }

            emit_byte(assembler, OP_BOR | 0xC0);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_test(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_TEST);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_ntest(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    emit_byte(assembler, OP_NTEST);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_jmp(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dest = read_number(assembler);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_JMP);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_jmpw(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dl = read_number(assembler);

    if(dl > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_JMPW);
    emit_word(assembler, dl);
    emit_byte(assembler, 0);
}

static void assemble_bjmp(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dest = read_number(assembler);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_BJMP);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_bjmpw(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dl = read_number(assembler);

    if(dl > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_BJMPW);
    emit_word(assembler, dl);
    emit_byte(assembler, 0);
}

static void assemble_call(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(dest > 249)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = read_number(assembler);

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_CALL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_ret(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dest;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        dest = read_reg(assembler);

        asm_parser_advance(assembler->parser);

        if(dest > 249)
            return;
    } else {
        if (assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
            asm_parser_error_at_current(assembler->parser, "Expected register, 'int', or 'byte'");
            return;
        }

        dest = read_number(assembler);

        if (dest > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
            return;
        }

        asm_parser_advance(assembler->parser);
    }

    emit_byte(assembler, OP_RET);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_dis(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    int64_t dest;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        dest = read_reg(assembler);

        asm_parser_advance(assembler->parser);

        if (dest > 249)
            return;
    } else {
        if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
            asm_parser_error_at_current(assembler->parser, "Expected register, 'int', or 'byte'");
            return;
        }

        dest = read_number(assembler);

        if(dest > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
            return;
        }

        asm_parser_advance(assembler->parser);
    }

    emit_byte(assembler, OP_DIS);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void emit_byte(Assembler* assembler, uint8_t byte) {
    chunk_write(&assembler->chunk, byte, 0);
}

static void emit_word(Assembler* assembler, uint16_t word) {
    emit_byte(assembler, ((uint8_t*) &word)[0]);
    emit_byte(assembler, ((uint8_t*) &word)[1]);
}

static uint8_t read_reg(Assembler* assembler) {
    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if (errno == ERANGE || num > 249) {
        asm_parser_error_at_current(assembler->parser, "Number is not a valid register (0-249)");
        return 251;
    }

    return (uint8_t) num;
}

static uint16_t read_const(Assembler* assembler) {
    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE || num > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is not a valid constant (0-65535)");
        return -1;
    }

    return (uint16_t) num;
}

static uint16_t read_byte(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return -1;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_BYTE) {
                asm_parser_error_at_current(assembler->parser, "Expected byte");
                return -1;
            }

            return index;
        }
    }

    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE || num > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is too large for type 'byte'");
        return -1;
    }

    return create_constant(assembler, BYTE_VALUE((uint8_t) num));
}

static uint16_t read_int(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return -1;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_INT) {
                asm_parser_error_at_current(assembler->parser, "Expected int");
                return -1;
            }

            return index;
        }
    }

    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE) {
        asm_parser_error_at_current(assembler->parser, "Number is too large for type 'int'");
        return -1;
    }

    return create_constant(assembler, INT_VALUE(num));
}

static uint16_t read_float(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return -1;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_FLOAT) {
                asm_parser_error_at_current(assembler->parser, "Expected float");
                return -1;
            }

            return index;
        }
    }

    double num = strtod(assembler->parser->current.start, NULL);

    if(errno == ERANGE) {
        asm_parser_error_at_current(assembler->parser, "Number is too small or too large for type 'float'");
        return -1;
    }

    return create_constant(assembler, FLOAT_VALUE(num));
}

static uint16_t read_string(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return (uint16_t) -1u;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(!value_is_dense_of_type(*value, DVAL_STRING)) {
                asm_parser_error_at_current(assembler->parser, "Expected string");
                return (uint16_t) -1u;
            }

            return index;
        }
    }

    if(assembler->parser->current.type != ASM_TOKEN_STRING) {
        asm_parser_error_at_current(assembler->parser, "Expected string");
        return (uint16_t) -1u;
    }

    const char* start = assembler->parser->current.start + 1;
    uint32_t length = assembler->parser->current.size - 2;

    const char* ptr = start;
    const char* end = start + length;

    uint32_t escapeCount = 0;

    while(ptr < end)
        if(*(ptr++) == '\\')
            ++escapeCount;

    char* str = (char*) MEM_ALLOC(length + 1 - escapeCount);
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
                        WARNING("Invalid escape sequence at index %d", assembler->parser->current.index + 1 + i);
                        break;
                }
                ++i;
            }
        } else str[index++] = start[i];
    }

    str[index] = '\0';

    start = str;
    length = index;
    uint32_t hash = map_hash(start, length);

    Assembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(super->strings, interned, NULL_VALUE);
    }

    MEM_FREE(str);

    return create_constant(assembler, DENSE_VALUE(interned));
}

static uint16_t read_identifier(Assembler* assembler) {
    uint32_t index = identifier_resolve(assembler, &assembler->parser->current);

    if(index > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
        return (uint16_t) - 1;
    }

    return (uint16_t) index;
}

static uint16_t read_any_const(Assembler* assembler) {
    switch(assembler->parser->current.type) {
        case ASM_TOKEN_CONSTANT:
            return read_const(assembler);
        case ASM_TOKEN_BYTE:
            return read_byte(assembler);
        case ASM_TOKEN_INT:
            return read_int(assembler);
        case ASM_TOKEN_FLOAT:
            return read_float(assembler);
        case ASM_TOKEN_STRING:
            return read_string(assembler);
        case ASM_TOKEN_IDENTIFIER:
            return read_identifier(assembler);
        default:
            asm_parser_error_at_current(assembler->parser, "Expected constant");
            return -1;
    }
}

static int64_t read_number(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = identifier_resolve(assembler, &assembler->parser->current);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return -1;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type == VAL_INT)
                return AS_INT(*value);
            return AS_BYTE(*value);
        }
    }

    if(assembler->parser->current.type == ASM_TOKEN_INT) {
        int64_t num = strtol(assembler->parser->current.start, NULL, 10);

        if (errno == ERANGE) {
            asm_parser_error_at_current(assembler->parser, "Number is too large for type 'int'");
            return -1;
        }

        return num;
    } else if(assembler->parser->current.type == ASM_TOKEN_BYTE){
        int64_t num = strtol(assembler->parser->current.start, NULL, 10);

        if(errno == ERANGE || num > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is too large for type 'byte'");
            return -1;
        }

        return num;
    } else {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return -1;
    }
}

static bool read_bool(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = identifier_resolve(assembler, &assembler->parser->current);

        if(index == (uint16_t) -1u) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return -1;
        } else {
            Value* value = &assembler->chunk.constants.values[index];
            return AS_BOOL(*value);
        }
    }

    return assembler->parser->current.type == ASM_TOKEN_TRUE;
}

static bool identifier_add(Assembler* assembler, const char* start, uint32_t length, uint16_t index) {
    if(map_find(&assembler->identifiers, start, length, map_hash(start, length)) == NULL) {
        map_set(&assembler->identifiers, dense_string_from(start, length), INT_VALUE(index));
        return true;
    } else return false;
}

// uint32_t max if the identifier does not exist.
static uint32_t identifier_resolve(Assembler* assembler, AsmToken* token) {
    Entry* entry = map_find_entry(&assembler->identifiers, token->start, token->size, map_hash(token->start, token->size));

    return entry == NULL ? UINT32_MAX : AS_INT(entry->value);
}

static uint16_t create_constant(Assembler* assembler, Value value) {
    size_t index = chunk_write_constant(&assembler->chunk, value);

    if(index > UINT16_MAX) {
        asm_parser_error_at_previous(assembler->parser, "Constant limit exceeded (65535)");
        return -1;
    }

    return (uint16_t) index;
}

static uint16_t create_string_constant(Assembler* assembler, const char* start, uint32_t length) {
    uint32_t hash = map_hash(start, length);

    Assembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(super->strings, interned, NULL_VALUE);
    }

    return create_constant(assembler, DENSE_VALUE(interned));
}

static DenseString* create_string_entry(Assembler* assembler, const char* start, uint32_t length) {
    uint32_t hash = map_hash(start, length);

    Assembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(super->strings, interned, NULL_VALUE);
    }

    return interned;
}