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
static void assemble_gt(Assembler*);
static void assemble_gte(Assembler*);
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

static void emit_byte(Assembler*, uint8_t);
static void emit_word(Assembler*, uint16_t);

static uint16_t create_constant(Assembler*, Value);
static uint16_t create_string_constant(Assembler*, const char*, uint32_t);

void assembler_init(Assembler* assembler) {
    assembler->super = NULL;
    assembler->strings = NULL;

    chunk_init(&assembler->chunk);
    map_init(&assembler->identifiers);

    assembler->canSwitchToData = true;
    assembler->mode = ASM_CODE;
}

void assembler_delete(Assembler* assembler) {
    map_delete(&assembler->identifiers);
}

AssemblerStatus assembler_assemble(Assembler* assembler, const char* str) {
    Parser parser;
    parser_init(&parser);

    assembler->parser = &parser;

    lexer_init(&assembler->parser->lexer);
    lexer_source(&assembler->parser->lexer, str);

    parser_advance(assembler->parser);

    while(assembler->parser->current.type != TOKEN_EOF) {
        assemble_mode_line(assembler);

        if(assembler->canSwitchToData)
            assembler->canSwitchToData = false;

        if(assembler->parser->panic)
            parser_sync(assembler->parser);
    }

    return assembler->parser->error ? ASSEMBLER_ERROR : ASSEMBLER_OK;
}

static void assemble_mode_line(Assembler* assembler) {
    if(assembler->parser->current.type == TOKEN_DOT) {
        parser_advance(assembler->parser);

        if(assembler->parser->current.type == TOKEN_DATA)
            assemble_data_mode_switch(assembler);
        else if(assembler->parser->current.type == TOKEN_CODE)
            assemble_code_mode_switch(assembler);
        else parser_error_at_current(assembler->parser, "Expected 'data' or 'code' after dot");
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
    if(!assembler->canSwitchToData) {
        parser_error_at_current(assembler->parser, "Cannot switch to data mode");
        return;
    } else if(assembler->mode == ASM_DATA) {
        parser_error_at_current(assembler->parser, "Assembler already is in data mode");
        return;
    } else assembler->mode = ASM_DATA;

    parser_advance(assembler->parser);
}

static void assemble_code_mode_switch(Assembler* assembler) {
    if(!assembler->canSwitchToData && assembler->mode == ASM_CODE) {
        parser_error_at_current(assembler->parser, "Assembler already is in code mode");
        return;
    } else assembler->mode = ASM_CODE;

    parser_advance(assembler->parser);
}

static void assemble_data_line(Assembler* assembler) {

}

static void assemble_code_line(Assembler* assembler) {
    switch(assembler->parser->current.type) {
        case TOKEN_CNST:
            assemble_cnst(assembler);
            break;
        case TOKEN_CNSTW:
            assemble_cnstw(assembler);
            break;
        case TOKEN_MOV:
            assemble_mov(assembler);
            break;
        case TOKEN_CLONE:
            assemble_clone(assembler);
            break;
        case TOKEN_DGLOB:
            assemble_dglob(assembler);
            break;
        case TOKEN_GGLOB:
            assemble_gglob(assembler);
            break;
        case TOKEN_SGLOB:
            assemble_sglob(assembler);
            break;
        case TOKEN_UPVAL:
            assemble_upval(assembler);
            break;
        case TOKEN_GUPVAL:
            assemble_gupval(assembler);
            break;
        case TOKEN_SUPVAL:
            assemble_supval(assembler);
            break;
        case TOKEN_CUPVAL:
            assemble_cupval(assembler);
            break;
        case TOKEN_CLSR:
            assemble_clsr(assembler);
            break;
        case TOKEN_ARR:
            assemble_arr(assembler);
            break;
        case TOKEN_PARR:
            assemble_parr(assembler);
            break;
        case TOKEN_LEN:
            assemble_len(assembler);
            break;
        case TOKEN_OBJ:
            assemble_obj(assembler);
            break;
        case TOKEN_GET:
            assemble_get(assembler);
            break;
        case TOKEN_SET:
            assemble_set(assembler);
            break;
        case TOKEN_NULL:
            assemble_null(assembler);
            break;
        case TOKEN_TRUE:
            assemble_true(assembler);
            break;
        case TOKEN_FALSE:
            assemble_false(assembler);
            break;
        case TOKEN_NOT:
            assemble_not(assembler);
            break;
        case TOKEN_BNOT:
            assemble_bnot(assembler);
            break;
        case TOKEN_NEG:
            assemble_neg(assembler);
            break;
        case TOKEN_INC:
            assemble_inc(assembler);
            break;
        case TOKEN_DEC:
            assemble_dec(assembler);
            break;
        case TOKEN_ADD:
            assemble_add(assembler);
            break;
        case TOKEN_SUB:
            assemble_sub(assembler);
            break;
        case TOKEN_MUL:
            assemble_mul(assembler);
            break;
        case TOKEN_DIV:
            assemble_div(assembler);
            break;
        case TOKEN_MOD:
            assemble_mod(assembler);
            break;
        case TOKEN_SHL:
            assemble_shl(assembler);
            break;
        case TOKEN_SHR:
            assemble_shr(assembler);
            break;
        case TOKEN_GT:
            assemble_gt(assembler);
            break;
        case TOKEN_GTE:
            assemble_gte(assembler);
            break;
        case TOKEN_LT:
            assemble_lt(assembler);
            break;
        case TOKEN_LTE:
            assemble_lte(assembler);
            break;
        case TOKEN_EQ:
            assemble_eq(assembler);
            break;
        case TOKEN_NEQ:
            assemble_neq(assembler);
            break;
        case TOKEN_BAND:
            assemble_band(assembler);
            break;
        case TOKEN_BXOR:
            assemble_bxor(assembler);
            break;
        case TOKEN_BOR:
            assemble_bor(assembler);
            break;
        case TOKEN_TEST:
            assemble_test(assembler);
            break;
        case TOKEN_NTEST:
            assemble_ntest(assembler);
            break;
        case TOKEN_JMP:
            assemble_jmp(assembler);
            break;
        case TOKEN_JMPW:
            assemble_jmpw(assembler);
            break;
        case TOKEN_BJMP:
            assemble_bjmp(assembler);
            break;
        case TOKEN_BJMPW:
            assemble_bjmpw(assembler);
            break;
        case TOKEN_CALL:
            assemble_call(assembler);
            break;
        case TOKEN_RET:
            assemble_ret(assembler);
            break;
        default:
            parser_error_at_current(assembler->parser, "Expected instruction");
            return;
    }
}

static void assemble_cnst(Assembler* assembler) {

}

static void assemble_cnstw(Assembler* assembler) {

}

static void assemble_mov(Assembler* assembler) {

}

static void assemble_clone(Assembler* assembler) {

}

static void assemble_dglob(Assembler* assembler) {

}

static void assemble_gglob(Assembler* assembler) {

}

static void assemble_sglob(Assembler* assembler) {

}

static void assemble_upval(Assembler* assembler) {

}

static void assemble_gupval(Assembler* assembler) {

}

static void assemble_supval(Assembler* assembler) {

}

static void assemble_cupval(Assembler* assembler) {

}

static void assemble_clsr(Assembler* assembler) {

}

static void assemble_arr(Assembler* assembler) {

}

static void assemble_parr(Assembler* assembler) {

}

static void assemble_len(Assembler* assembler) {

}

static void assemble_obj(Assembler* assembler) {

}

static void assemble_get(Assembler* assembler) {

}

static void assemble_set(Assembler* assembler) {

}

static void assemble_null(Assembler* assembler) {

}

static void assemble_true(Assembler* assembler) {

}

static void assemble_false(Assembler* assembler) {

}

static void assemble_not(Assembler* assembler) {

}

static void assemble_bnot(Assembler* assembler) {

}

static void assemble_neg(Assembler* assembler) {

}

static void assemble_inc(Assembler* assembler) {

}

static void assemble_dec(Assembler* assembler) {

}

static void assemble_add(Assembler* assembler) {

}

static void assemble_sub(Assembler* assembler) {

}

static void assemble_mul(Assembler* assembler) {

}

static void assemble_div(Assembler* assembler) {

}

static void assemble_mod(Assembler* assembler) {

}

static void assemble_shl(Assembler* assembler) {

}

static void assemble_shr(Assembler* assembler) {

}

static void assemble_gt(Assembler* assembler) {

}

static void assemble_gte(Assembler* assembler) {

}

static void assemble_lt(Assembler* assembler) {

}

static void assemble_lte(Assembler* assembler) {

}

static void assemble_eq(Assembler* assembler) {

}

static void assemble_neq(Assembler* assembler) {

}

static void assemble_band(Assembler* assembler) {

}

static void assemble_bxor(Assembler* assembler) {

}

static void assemble_bor(Assembler* assembler) {

}

static void assemble_test(Assembler* assembler) {

}

static void assemble_ntest(Assembler* assembler) {

}

static void assemble_jmp(Assembler* assembler) {

}

static void assemble_jmpw(Assembler* assembler) {

}

static void assemble_bjmp(Assembler* assembler) {

}

static void assemble_bjmpw(Assembler* assembler) {

}

static void assemble_call(Assembler* assembler) {

}

static void assemble_ret(Assembler* assembler) {

}

static void emit_byte(Assembler* assembler, uint8_t byte) {
    chunk_write(&assembler->chunk, byte, 0);
}

static void emit_word(Assembler* assembler, uint16_t word) {
    emit_byte(assembler, ((uint8_t*) &word)[0]);
    emit_byte(assembler, ((uint8_t*) &word)[1]);
}

static uint16_t create_constant(Assembler* assembler, Value value) {
    size_t index = chunk_write_constant(&assembler->chunk, value);

    if(index > UINT16_MAX) {
        parser_error_at_previous(assembler->parser, "Constant limit exceeded (65535)");
        return 0;
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