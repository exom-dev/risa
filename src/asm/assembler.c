#include "assembler.h"

#include "../io/log.h"

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

// The functions with the '_instruction' suffix are shared between multiple instructions.
static void assemble_global_instruction(Assembler*);   // Takes a string and a register/constant.
static void assemble_cnst_instruction(Assembler*);     // Takes a register and a constant.
static void assemble_copy_instruction(Assembler*);     // Takes two registers.
static void assemble_producer_instruction(Assembler*); // Takes a register.
static void assemble_unary_instruction(Assembler*);    // Takes a register and a register/constant.
static void assemble_binary_instruction(Assembler*);   // Takes a register and two registers/constants.
static void assemble_jump_instruction(Assembler*);     // Takes a number, either byte or word.
static void assemble_consumer_instruction(Assembler*); // Takes a register or RISA_TODLR_REGISTER_NULL.
static void assemble_call(Assembler*);                 // Takes a register and a number.
static void assemble_gglob(Assembler*);                // Takes a register and a string.
static void assemble_upval(Assembler*);                // Takes two numbers and a bool.
static void assemble_gupval(Assembler*);               // Takes a register and two numbers.
static void assemble_supval(Assembler*);               // Takes two numbers and a register.
static void assemble_clsr(Assembler*);                 // Takes two registers and a number.
static void assemble_get(Assembler*);                  // Takes two registers and a register/constant.
static void assemble_set(Assembler*);                  // Takes a register and two registers/constants.

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
    risa_io_init(&assembler->io);
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
    risa_io_clone(&parser.io, &assembler->io); // Use the same IO interface for the parser.

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
        case ASM_TOKEN_CNSTW:
            assemble_cnst_instruction(assembler);
            break;
        case ASM_TOKEN_MOV:
        case ASM_TOKEN_CLONE:
        case ASM_TOKEN_LEN:
            assemble_copy_instruction(assembler);
            break;
        case ASM_TOKEN_DGLOB:
        case ASM_TOKEN_SGLOB:
            assemble_global_instruction(assembler);
            break;
        case ASM_TOKEN_GGLOB:
            assemble_gglob(assembler);
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
        case ASM_TOKEN_CLSR:
            assemble_clsr(assembler);
            break;
        case ASM_TOKEN_GET:
            assemble_get(assembler);
            break;
        case ASM_TOKEN_SET:
            assemble_set(assembler);
            break;
        case ASM_TOKEN_NULL:
        case ASM_TOKEN_TRUE:
        case ASM_TOKEN_FALSE:
        case ASM_TOKEN_ARR:
        case ASM_TOKEN_OBJ:
        case ASM_TOKEN_ACC:
        case ASM_TOKEN_INC:
        case ASM_TOKEN_DEC:
        case ASM_TOKEN_TEST:
        case ASM_TOKEN_NTEST:
        case ASM_TOKEN_CUPVAL:
            assemble_producer_instruction(assembler);
            break;
        case ASM_TOKEN_NOT:
        case ASM_TOKEN_BNOT:
        case ASM_TOKEN_NEG:
        case ASM_TOKEN_PARR:
            assemble_unary_instruction(assembler);
            break;
        case ASM_TOKEN_ADD:
        case ASM_TOKEN_SUB:
        case ASM_TOKEN_MUL:
        case ASM_TOKEN_DIV:
        case ASM_TOKEN_MOD:
        case ASM_TOKEN_SHL:
        case ASM_TOKEN_SHR:
        case ASM_TOKEN_LT:
        case ASM_TOKEN_LTE:
        case ASM_TOKEN_EQ:
        case ASM_TOKEN_NEQ:
        case ASM_TOKEN_BAND:
        case ASM_TOKEN_BXOR:
        case ASM_TOKEN_BOR:
            assemble_binary_instruction(assembler);
            break;
        case ASM_TOKEN_JMP:
        case ASM_TOKEN_JMPW:
        case ASM_TOKEN_BJMP:
        case ASM_TOKEN_BJMPW:
            assemble_jump_instruction(assembler);
            break;
        case ASM_TOKEN_CALL:
            assemble_call(assembler);
            break;
        case ASM_TOKEN_RET:
        case ASM_TOKEN_DIS:
            assemble_consumer_instruction(assembler);
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

        if(index == UINT16_MAX)
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

static void assemble_global_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    uint16_t dest = read_string(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    if(dest > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
        return;
    }

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        emit_byte(assembler, op);
    } else {
        left = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_cnst_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);
    bool isWord = (op == OP_CNSTW);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t left = read_any_const(assembler);

    if(isWord) {
        if(left > UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-65535)");
            return;
        }
    } else {
        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255); consider using 'CNSTW'");
            return;
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, op);
    emit_byte(assembler, (uint8_t) dest);

    if(isWord) {
        emit_word(assembler, left);
    } else {
        emit_byte(assembler, (uint8_t) left);
        emit_byte(assembler, 0);
    }
}

static void assemble_copy_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, op);
    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, 0);
}

static void assemble_producer_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, op);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_unary_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        emit_byte(assembler, op);
    } else {
        left = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, 0);
}

static void assemble_binary_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t left;
    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(assembler->parser->panic)
                return;

            emit_byte(assembler, op);
        } else {
            right = read_any_const(assembler);

            if(assembler->parser->panic)
                return;

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
                return;
            }

            emit_byte(assembler, op | RISA_TODLR_TYPE_RIGHT_MASK);
        }
    } else {
        left = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        asm_parser_advance(assembler->parser);

        if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
            right = read_reg(assembler);

            if(assembler->parser->panic)
                return;

            emit_byte(assembler, op | RISA_TODLR_TYPE_LEFT_MASK);
        } else {
            right = read_any_const(assembler);

            if(assembler->parser->panic)
                return;

            if(right > UINT8_MAX) {
                asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
                return;
            }

            emit_byte(assembler, op | RISA_TODLR_TYPE_MASK);
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_jump_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);
    bool isWord = (op == OP_JMPW || op == OP_BJMPW); // Word variation: 0-65535 instead of 0-255

    asm_parser_advance(assembler->parser);

    int64_t dest = read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(isWord) {
        if(dest > UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is out of range (0-65535)");
            return;
        }
    } else {
        if(dest > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
            return;
        }
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, op);

    if(isWord) {
        emit_word(assembler, (uint16_t) dest);
    } else {
        emit_byte(assembler, (uint8_t) dest);
        emit_byte(assembler, 0);
    }

    emit_byte(assembler, 0);
}

static void assemble_consumer_instruction(Assembler* assembler) {
    uint8_t op = asm_token_to_opcode(assembler->parser->current.type);

    asm_parser_advance(assembler->parser);

    int64_t dest;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        dest = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        asm_parser_advance(assembler->parser);
    } else {
        if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
            asm_parser_error_at_current(assembler->parser, "Expected register or number '" RISA_TODLR_REGISTER_NULL_STR "'");
            return;
        }

        dest = read_number(assembler);

        if(assembler->parser->panic)
            return;

        if(dest != RISA_TODLR_REGISTER_NULL) {
            asm_parser_error_at_current(assembler->parser, "Unexpected number value; must be " RISA_TODLR_REGISTER_NULL_STR);
            return;
        }

        asm_parser_advance(assembler->parser);
    }

    emit_byte(assembler, op);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, 0);
    emit_byte(assembler, 0);
}

static void assemble_call(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_CALL);
    emit_byte(assembler, dest);
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

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t left = read_string(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_GGLOB);
    emit_byte(assembler, dest);
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

    if(assembler->parser->panic)
        return;

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

    if(assembler->parser->panic)
        return;

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

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    if(assembler->parser->current.type != ASM_TOKEN_INT && assembler->parser->current.type != ASM_TOKEN_BYTE) {
        asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
        return;
    }

    int64_t left = read_number(assembler);

    if(assembler->parser->panic)
        return;

    if(left > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is out of range (0-255)");
        return;
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_GUPVAL);
    emit_byte(assembler, dest);
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

    if(assembler->parser->panic)
        return;

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

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, OP_SUPVAL);
    emit_byte(assembler, (uint8_t) dest);
    emit_byte(assembler, left);
    emit_byte(assembler, 0);
}

static void assemble_clsr(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

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
    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_get(Assembler* assembler) {
    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t left = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        right = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        emit_byte(assembler, OP_GET);
    } else {
        right = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(right > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        emit_byte(assembler, OP_GET | RISA_TODLR_TYPE_RIGHT_MASK);
    }

    asm_parser_advance(assembler->parser);

    emit_byte(assembler, dest);
    emit_byte(assembler, left);
    emit_byte(assembler, (uint8_t) right);
}

static void assemble_set(Assembler* assembler) {
    bool isLeftConst;

    asm_parser_advance(assembler->parser);

    if(assembler->parser->current.type != ASM_TOKEN_REGISTER) {
        asm_parser_error_at_current(assembler->parser, "Expected register");
        return;
    }

    uint8_t dest = read_reg(assembler);

    if(assembler->parser->panic)
        return;

    asm_parser_advance(assembler->parser);

    uint16_t left;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        left = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        isLeftConst = false;
    } else {
        left = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(left > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        isLeftConst = true;
    }

    asm_parser_advance(assembler->parser);

    uint16_t right;

    if(assembler->parser->current.type == ASM_TOKEN_REGISTER) {
        right = read_reg(assembler);

        if(assembler->parser->panic)
            return;

        if(isLeftConst)
            emit_byte(assembler, OP_SET | RISA_TODLR_TYPE_LEFT_MASK);
        else emit_byte(assembler, OP_SET);
    } else {
        right = read_any_const(assembler);

        if(assembler->parser->panic)
            return;

        if(right > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Constant index is too large (0-255)");
            return;
        }

        if(isLeftConst)
            emit_byte(assembler, OP_SET | RISA_TODLR_TYPE_MASK);
        else emit_byte(assembler, OP_SET | RISA_TODLR_TYPE_RIGHT_MASK);
    }

    asm_parser_advance(assembler->parser);

    if(assembler->parser->panic)
        return;

    emit_byte(assembler, dest);
    emit_byte(assembler, (uint8_t) left);
    emit_byte(assembler, (uint8_t) right);
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
        return RISA_TODLR_REGISTER_NULL;
    }

    return (uint8_t) num;
}

static uint16_t read_const(Assembler* assembler) {
    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE || num > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is not a valid constant (0-65535)");
        return UINT16_MAX;
    }

    return (uint16_t) num;
}

static uint16_t read_byte(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_BYTE) {
                asm_parser_error_at_current(assembler->parser, "Expected byte");
                return UINT16_MAX;
            }

            return index;
        }
    }

    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE || num > UINT8_MAX) {
        asm_parser_error_at_current(assembler->parser, "Number is too large for type 'byte'");
        return UINT16_MAX;
    }

    return create_constant(assembler, BYTE_VALUE((uint8_t) num));
}

static uint16_t read_int(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_INT) {
                asm_parser_error_at_current(assembler->parser, "Expected int");
                return UINT16_MAX;
            }

            return index;
        }
    }

    int64_t num = strtol(assembler->parser->current.start, NULL, 10);

    if(errno == ERANGE) {
        asm_parser_error_at_current(assembler->parser, "Number is too large for type 'int'");
        return UINT16_MAX;
    }

    return create_constant(assembler, INT_VALUE(num));
}

static uint16_t read_float(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(value->type != VAL_FLOAT) {
                asm_parser_error_at_current(assembler->parser, "Expected float");
                return UINT16_MAX;
            }

            return index;
        }
    }

    double num = strtod(assembler->parser->current.start, NULL);

    if(errno == ERANGE) {
        asm_parser_error_at_current(assembler->parser, "Number is too small or too large for type 'float'");
        return UINT16_MAX;
    }

    return create_constant(assembler, FLOAT_VALUE(num));
}

static uint16_t read_string(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = read_identifier(assembler);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return UINT16_MAX;
        } else {
            Value* value = &assembler->chunk.constants.values[index];

            if(!value_is_dense_of_type(*value, DVAL_STRING)) {
                asm_parser_error_at_current(assembler->parser, "Expected string");
                return UINT16_MAX;
            }

            return index;
        }
    }

    if(assembler->parser->current.type != ASM_TOKEN_STRING) {
        asm_parser_error_at_current(assembler->parser, "Expected string");
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
    uint32_t hash = map_hash(start, length);

    Assembler* super = assembler->super == NULL ? assembler : assembler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(super->strings, interned, NULL_VALUE);
    }

    RISA_MEM_FREE(str);

    return create_constant(assembler, DENSE_VALUE(interned));
}

static uint16_t read_identifier(Assembler* assembler) {
    uint32_t index = identifier_resolve(assembler, &assembler->parser->current);

    if(index > UINT16_MAX) {
        asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
        return UINT16_MAX;
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
            return UINT16_MAX;
    }
}

static int64_t read_number(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = identifier_resolve(assembler, &assembler->parser->current);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return INT64_MAX;
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
            return INT64_MAX;
        }

        return num;
    }

    if(assembler->parser->current.type == ASM_TOKEN_BYTE){
        int64_t num = strtol(assembler->parser->current.start, NULL, 10);

        if(errno == ERANGE || num > UINT8_MAX) {
            asm_parser_error_at_current(assembler->parser, "Number is too large for type 'byte'");
            return INT64_MAX;
        }

        return num;
    }

    asm_parser_error_at_current(assembler->parser, "Expected 'int' or 'byte'");
    return INT64_MAX;
}

static bool read_bool(Assembler* assembler) {
    if(assembler->parser->current.type == ASM_TOKEN_IDENTIFIER) {
        uint16_t index = identifier_resolve(assembler, &assembler->parser->current);

        if(index == UINT16_MAX) {
            asm_parser_error_at_current(assembler->parser, "Identifier does not exist");
            return false;
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