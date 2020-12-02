#include "compiler.h"
#include "parser.h"

#include "../value/dense.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../lexer/lexer.h"

#include "../asm/assembler.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define INSTRUCTION_MASK 0x3F

static void compile_byte(Compiler*, bool);
static void compile_int(Compiler*, bool);
static void compile_float(Compiler*, bool);
static void compile_string(Compiler*, bool);
static void compile_literal(Compiler*, bool);
static void compile_identifier(Compiler*, bool);
static void compile_array(Compiler*, bool);
static void compile_object(Compiler*, bool);
static void compile_declaration(Compiler*);
static void compile_variable_declaration(Compiler*);
static void compile_function_declaration(Compiler*);
static void compile_function(Compiler*);
static void compile_statement(Compiler*);
static void compile_if_statement(Compiler*);
static void compile_while_statement(Compiler*);
static void compile_for_statement(Compiler*);
static void compile_return_statement(Compiler*);
static void compile_continue_statement(Compiler*);
static void compile_break_statement(Compiler*);
static void compile_block(Compiler*);
static void compile_inline_asm_statement(Compiler*);
static void compile_disasm_statement(Compiler*);
static void compile_expression_statement(Compiler*);
static void compile_expression(Compiler*);
static void compile_expression_precedence(Compiler*, Precedence);
static void compile_return_expression(Compiler*);
static void compile_call(Compiler*, bool);
static void compile_clone(Compiler*, bool);
static void compile_dot(Compiler*, bool);
static void compile_grouping_or_lambda(Compiler*, bool);
static void compile_lambda(Compiler*);
static void compile_accessor(Compiler*, bool);
static void compile_unary(Compiler*, bool);
static void compile_binary(Compiler*, bool);
static void compile_ternary(Compiler*, bool);
static void compile_equal_op(Compiler*, bool);
static void compile_prefix(Compiler*, bool);
static void compile_postfix(Compiler*, bool);
static void compile_and(Compiler*, bool);
static void compile_or(Compiler*, bool);
static void compile_comma(Compiler*, bool);

static uint8_t compile_arguments(Compiler*);

static void scope_begin(Compiler*);
static void scope_end(Compiler*);

static void    local_add(Compiler*, Token);
static uint8_t local_resolve(Compiler*, Token*);

static uint8_t upvalue_add(Compiler*, uint8_t, bool);
static uint8_t upvalue_resolve(Compiler*, Token*);

static void     emit_byte(Compiler*, uint8_t);
static void     emit_bytes(Compiler*, uint8_t, uint8_t, uint8_t);
static void     emit_word(Compiler*, uint16_t);
static void     emit_constant(Compiler*, Value);
static void     emit_return(Compiler*);
static void     emit_mov(Compiler*, uint8_t, uint8_t);
static void     emit_jump(Compiler*, uint32_t);
static void     emit_backwards_jump(Compiler*, uint32_t);
static void     emit_backwards_jump_from(Compiler*, uint32_t, uint32_t);
static uint32_t emit_blank(Compiler*);

static uint16_t create_constant(Compiler*, Value);
static uint16_t create_identifier_constant(Compiler*);
static uint16_t create_string_constant(Compiler*, const char*, uint32_t);
static uint16_t declare_variable(Compiler*);

static bool    register_reserve(Compiler*);
static uint8_t register_find(Compiler*, RegType, Token);
static void    register_free(Compiler*);

static void finalize_compilation(Compiler*);

Rule EXPRESSION_RULES[] = {
        {compile_grouping_or_lambda, compile_call,  PREC_CALL },        // TOKEN_LEFT_PAREN
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_RIGHT_PAREN
        { compile_array,     compile_accessor,      PREC_CALL },        // TOKEN_LEFT_BRACKET
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_RIGHT_BRACKET
        { compile_object,     NULL,                 PREC_NONE },        // TOKEN_LEFT_BRACE
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_RIGHT_BRACE
        { NULL,     compile_comma,                  PREC_COMMA },       // TOKEN_COMMA
        { NULL,     compile_dot,                    PREC_CALL },        // TOKEN_DOT
        { compile_unary,    compile_binary,         PREC_TERM },        // TOKEN_MINUS
        { compile_prefix,    compile_postfix,       PREC_CALL },        // TOKEN_MINUS_MINUS
        { NULL,     compile_binary,                 PREC_TERM },        // TOKEN_PLUS
        { compile_prefix,     compile_postfix,      PREC_CALL },        // TOKEN_PLUS_PLUS
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_COLON
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_SEMICOLON
        { NULL,     compile_binary,                 PREC_FACTOR },      // TOKEN_SLASH
        { NULL,     compile_binary,                 PREC_FACTOR },      // TOKEN_STAR
        { compile_unary,     NULL,                  PREC_NONE },        // TOKEN_TILDE
        { NULL,     compile_binary,                 PREC_BITWISE_XOR }, // TOKEN_AMPERSAND
        { NULL,     compile_binary,                 PREC_FACTOR },      // TOKEN_PERCENT
        { NULL,     compile_ternary,                PREC_TERNARY },     // TOKEN_QUESTION
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_DOLLAR
        { compile_unary,     NULL,                  PREC_NONE },        // TOKEN_BANG
        { NULL,     compile_binary,                 PREC_EQUALITY },    // TOKEN_BANG_EQUAL
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_EQUAL
        { NULL,     compile_binary,                 PREC_EQUALITY },    // TOKEN_EQUAL_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_EQUAL_GREATER
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_PLUS_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_MINUS_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_STAR_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_SLASH_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_CARET_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_PERCENT_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_PIPE_EQUAL
        { NULL,     compile_equal_op,               PREC_ASSIGNMENT },  // TOKEN_AMPERSAND_EQUAL
        { NULL,     compile_binary,                 PREC_COMPARISON },  // TOKEN_GREATER
        { NULL,     compile_binary,                 PREC_COMPARISON },  // TOKEN_GREATER_EQUAL
        { NULL,     compile_binary,                 PREC_SHIFT },       // TOKEN_GREATER_GREATER
        { NULL,     compile_binary,                 PREC_COMPARISON },  // TOKEN_LESS
        { NULL,     compile_binary,                 PREC_COMPARISON },  // TOKEN_LESS_EQUAL
        { NULL,     compile_binary,                 PREC_SHIFT },       // TOKEN_LESS_LESS
        { NULL,     compile_binary,                 PREC_BITWISE_AND }, // TOKEN_AMPERSAND
        { NULL,     compile_and,                    PREC_AND },         // TOKEN_AMPERSAND_AMPERSAND
        { NULL,     compile_binary,                 PREC_BITWISE_OR },  // TOKEN_PIPE
        { NULL,     compile_or,                     PREC_OR },          // TOKEN_PIPE_PIPE
        { compile_identifier,     NULL,             PREC_NONE },        // TOKEN_IDENTIFIER
        { compile_string,     NULL,                 PREC_NONE },        // TOKEN_STRING
        { compile_byte,     NULL,                   PREC_NONE },        // TOKEN_BYTE
        { compile_int,     NULL,                    PREC_NONE },        // TOKEN_INT
        { compile_float,     NULL,                  PREC_NONE },        // TOKEN_FLOAT
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_IF
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_ELSE
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_WHILE
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_FOR
        { compile_literal,     NULL,                PREC_NONE },        // TOKEN_TRUE
        { compile_literal,     NULL,                PREC_NONE },        // TOKEN_FALSE
        { compile_literal,     NULL,                PREC_NONE },        // TOKEN_NULL
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_VAR
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_FUNCTION
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_RETURN
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_CONTINUE
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_BREAK
        { compile_clone,     NULL,                  PREC_NONE },        // TOKEN_CLONE
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_ERROR
        { NULL,     NULL,                           PREC_NONE },        // TOKEN_EOF
};

void compiler_init(Compiler* compiler) {
    compiler->super = NULL;
    compiler->function = dense_function_create();

    chunk_init(&compiler->function->chunk);
    map_init(&compiler->strings);

    compiler->regIndex = 0;
    compiler->options.replMode = false;
    compiler->last.reg = 0;
    compiler->last.isNew = false;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.canOverwrite = false; // true
    compiler->last.lvalMeta.type = LVAL_LOCAL;
    compiler->last.lvalMeta.global = 0;
    compiler->last.lvalMeta.globalReg = 0;
    compiler->last.lvalMeta.propOrigin = 0;
    compiler->last.lvalMeta.propIndex.as.reg = 0;
    compiler->last.lvalMeta.propIndex.isConst = false;

    compiler->localCount = 0;
    compiler->upvalueCount = 0;
    compiler->loopCount = 0;
    compiler->leapCount = 0;

    compiler->scopeDepth = 0;
}

void compiler_delete(Compiler* compiler) {
    map_delete(&compiler->strings);
}

CompilerStatus compiler_compile(Compiler* compiler, const char* str) {
    Parser parser;
    parser_init(&parser);

    compiler->parser = &parser;

    lexer_init(&compiler->parser->lexer);
    lexer_source(&compiler->parser->lexer, str);

    parser_advance(compiler->parser);

    while(compiler->parser->current.type != TOKEN_EOF) {
        compile_declaration(compiler);

        if(compiler->options.replMode) {
            emit_byte(compiler, OP_ACC);
            emit_byte(compiler, compiler->last.reg);
            emit_byte(compiler, 0);
            emit_byte(compiler, 0);
        }
    }

    finalize_compilation(compiler);

    return compiler->parser->error ? COMPILER_ERROR : COMPILER_OK;
}

static void compile_byte(Compiler* compiler, bool allowAssignment) {
    uint8_t reg = register_find(compiler, REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!register_reserve(compiler))
            return;

        int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

        if (errno == ERANGE || num > 255) {
            parser_error_at_previous(compiler->parser, "Number is too large for type 'byte'");
            return;
        }

        emit_constant(compiler, BYTE_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, compiler->parser->previous };
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    }
}

static void compile_int(Compiler* compiler, bool allowAssignment) {
    uint8_t reg = register_find(compiler, REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if(!register_reserve(compiler))
            return;

        int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

        if(errno == ERANGE) {
            parser_error_at_previous(compiler->parser, "Number is too large for type 'int'");
            return;
        }

        emit_constant(compiler, INT_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, compiler->parser->previous };
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    }
}

static void compile_float(Compiler* compiler, bool allowAssignment) {
    uint8_t reg = register_find(compiler, REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if(!register_reserve(compiler))
            return;

        double num = strtod(compiler->parser->previous.start, NULL);

        if(errno == ERANGE) {
            parser_error_at_previous(compiler->parser, "Number is too small or too large for type 'float'");
            return;
        }

        emit_constant(compiler, FLOAT_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, compiler->parser->previous };
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    }
}

static void compile_string(Compiler* compiler, bool allowAssignment) {
    uint8_t reg = register_find(compiler, REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!register_reserve(compiler))
            return;

        const char *start = compiler->parser->previous.start + 1;
        uint32_t length = compiler->parser->previous.size - 2;

        const char *ptr = start;
        const char *end = start + length;

        uint32_t escapeCount = 0;

        while (ptr < end)
            if (*(ptr++) == '\\')
                ++escapeCount;

        char *str = (char *) MEM_ALLOC(length + 1 - escapeCount);
        uint32_t index = 0;

        for (uint32_t i = 0; i < length; ++i) {
            if (start[i] == '\\') {
                if (i < length) {
                    switch (start[i + 1]) {
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
                            WARNING("Invalid escape sequence at index %d", compiler->parser->previous.index + 1 + i);
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

        Compiler *super = compiler->super == NULL ? compiler : compiler->super;

        while (super->super != NULL)
            super = super->super;

        DenseString *interned = map_find(&super->strings, start, length, hash);

        if (interned == NULL) {
            interned = dense_string_from(start, length);
            map_set(&super->strings, interned, NULL_VALUE);
        }

        MEM_FREE(str);

        emit_constant(compiler, DENSE_VALUE(interned));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, compiler->parser->previous };
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    }
}

static void compile_literal(Compiler* compiler, bool allowAssignment) {
    uint8_t reg = register_find(compiler, REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!register_reserve(compiler))
            return;

        switch (compiler->parser->previous.type) {
            case TOKEN_NULL:
                emit_byte(compiler, OP_NULL);
                break;
            case TOKEN_TRUE:
                emit_byte(compiler, OP_TRUE);
                break;
            case TOKEN_FALSE:
                emit_byte(compiler, OP_FALSE);
                break;
            default:
                return;
        }

        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, compiler->parser->previous };
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
    }
}

static void compile_identifier(Compiler* compiler, bool allowAssignment) {
    uint8_t get;
    uint8_t set;

    uint8_t index = local_resolve(compiler, &compiler->parser->previous);

    if(index != 251) {
        get = OP_MOV;
        set = OP_MOV;
    } else {
        index = upvalue_resolve(compiler, &compiler->parser->previous);

        if(index != 251) {
            get = OP_GUPVAL;
            set = OP_SUPVAL;
        } else {
            index = create_identifier_constant(compiler);

            get = OP_GGLOB;
            set = OP_SGLOB;
        }
    }

    if(allowAssignment && (compiler->parser->current.type == TOKEN_EQUAL)) {
        parser_advance(compiler->parser);

        uint32_t chunkSize = compiler->function->chunk.size;
        compile_expression(compiler);
        Chunk chunk = compiler->function->chunk;

        if(set == OP_MOV) {
            if((chunkSize == chunk.size)                  // Register reference; no new OPs.
            || (chunkSize + 4 == chunk.size               // Only one OP.
              && (chunk.bytecode[chunkSize] == OP_INC     // OP is INC.
              || (chunk.bytecode[chunkSize] == OP_DEC)))) // OP is DEC.
                emit_mov(compiler, index, compiler->last.reg); // MOV the origin.

            else if(compiler->last.isPostIncrement) {
                int32_t incOffset = 4;

                while(chunk.size - incOffset >= chunkSize && chunk.bytecode[chunk.size - incOffset] != OP_INC && chunk.bytecode[chunk.size - incOffset] != OP_DEC)
                    incOffset += 4;

                if(chunk.size - incOffset < chunkSize) {
                    parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix when it isn't (report this to the developers)");
                    return;
                }

                if(chunk.size - incOffset - 4 < chunkSize || chunk.bytecode[chunk.size - incOffset - 4] != OP_MOV) {
                    parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not MOV (report this to the developers)");
                    return;
                }

                if(chunk.bytecode[chunk.size - incOffset + 1] == compiler->localCount - 1) { // The INC target interferes with the local register.
                    uint8_t dest = compiler->localCount - 1;
                    uint8_t tmp = chunk.bytecode[chunk.size - incOffset - 4 + 1]; // Original MOV target.

                    chunk.bytecode[chunk.size - incOffset - 4 + 2] = tmp;  // MOV source.
                    chunk.bytecode[chunk.size - incOffset - 4 + 1] = dest; // MOV target.
                    chunk.bytecode[chunk.size - incOffset + 1] = tmp;      // INC target.

                    incOffset += 8; // Jump over MOV.

                    if(chunk.size - incOffset >= chunkSize) {
                        if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_GET) {
                            parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not GET (report this to the developers)");
                            return;
                        }

                        chunk.bytecode[chunk.size - incOffset + 1] = tmp;
                    }

                    incOffset -= 4 + 8;

                    if(incOffset > 0) {
                        if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_SET) {
                            parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC successor is not SET (report this to the developers)");
                            return;
                        }

                        chunk.bytecode[chunk.size - incOffset + 3] = tmp;
                    }

                    incOffset -= 4;

                    if(incOffset > 0) {
                        if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_SGLOB) {
                            parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but SET successor is not SGLOB (report this to the developers)");
                            return;
                        }

                        chunk.bytecode[chunk.size - incOffset + 2] = tmp;
                    }
                } else chunk.bytecode[chunk.size - incOffset - 4 + 1] = compiler->localCount - 1; // Directly MOV to local.
            } else if(op_has_direct_dest(chunk.bytecode[chunk.size - 4] & INSTRUCTION_MASK)&& !compiler->last.isEqualOp) { // Can directly assign to local.
                chunk.bytecode[chunk.size - 4 + 1] = index;  // Do it.
                compiler->last.reg = index;
            } else compiler->function->chunk.bytecode[compiler->function->chunk.size - 3] = index;
        } else {
            if(set == OP_SGLOB) {
                Chunk* chunk = &compiler->function->chunk;
                if(chunk->bytecode[chunk->size - 4] == OP_CNST) {
                    compiler->last.reg = chunk->bytecode[chunk->size - 2];
                    compiler->last.isConst = true;
                    chunk->size -= 4;
                }

                #define L_TYPE (compiler->last.isConst * 0x80)
                set |= L_TYPE;
                #undef L_TYPE
            }

            emit_byte(compiler, set);
            emit_byte(compiler, index);
            emit_byte(compiler, compiler->last.reg); // compiler->regIndex - 1;
            emit_byte(compiler, 0);
        }

        compiler->last.isConst = false;

        //register_free(compiler);
    } else {
        uint8_t reg = register_find(compiler, get == OP_MOV ? REG_LOCAL : (get == OP_GUPVAL ? REG_UPVAL : REG_GLOBAL), compiler->parser->previous);

        // Eliminate GGLOB after DGLOB
        if(reg == 251) {
            Chunk *chunk = &compiler->function->chunk;

            if(chunk->size > 0 && chunk->bytecode[chunk->size - 4] == OP_DGLOB)
                if(chunk->bytecode[chunk->size - 4 + 1] == index)
                    reg = chunk->bytecode[chunk->size - 4 + 2];
        }

        if(reg == 251) {
            if(!register_reserve(compiler))
                return;

            if(get == OP_MOV)
                emit_mov(compiler, compiler->regIndex - 1, index);
            else {
                emit_byte(compiler, get);
                emit_byte(compiler, compiler->regIndex - 1);
                emit_byte(compiler, index);
                emit_byte(compiler, 0);
            }

            compiler->last.reg = compiler->regIndex - 1;
            compiler->regs[compiler->last.reg] = (RegInfo) { get == OP_MOV ? REG_LOCAL : (get == OP_GUPVAL ? REG_UPVAL : REG_GLOBAL), compiler->parser->previous };
            compiler->last.isNew = true;
        } else {
            compiler->last.reg = reg;

            if(reg == compiler->regIndex) {
                if (!register_reserve(compiler))
                    return;
                compiler->last.isNew = true;
            } else compiler->last.isNew = false;
        }

        switch(get) {
            case OP_MOV:
                compiler->last.lvalMeta.type = LVAL_LOCAL;
                break;
            case OP_GGLOB:
                compiler->last.lvalMeta.type = LVAL_GLOBAL;
                compiler->last.lvalMeta.global = index;
                compiler->last.lvalMeta.globalReg = reg == 251 ? compiler->regIndex - 1 : reg;
                break;
            case OP_GUPVAL:
                compiler->last.lvalMeta.type = LVAL_UPVAL;
                compiler->last.lvalMeta.upval = index;
                break;
            default:
                break;
        }
    }

    compiler->last.isConst = false;
    compiler->last.isLvalue = true;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
}

static void compile_array(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    uint8_t reg = compiler->regIndex - 1;

    emit_byte(compiler, OP_ARR);
    emit_byte(compiler, reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->parser->current.type != TOKEN_RIGHT_BRACKET) {
        while (1) {
            compile_expression_precedence(compiler, PREC_COMMA + 1);

            if (compiler->last.isNew)
                register_free(compiler);

            if (compiler->last.isConst) {
                compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 2];
                compiler->function->chunk.size -= 4;
            }

            #define L_TYPE (compiler->last.isConst * 0x80)

            emit_byte(compiler, OP_PARR | L_TYPE);
            emit_byte(compiler, reg);
            emit_byte(compiler, compiler->last.reg);
            emit_byte(compiler, 0);

            #undef L_TYPE

            if (compiler->parser->current.type == TOKEN_RIGHT_BRACKET || compiler->parser->current.type == TOKEN_EOF)
                break;

            parser_advance(compiler->parser);
        }
    }

    parser_consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expected ']' after array contents");

    compiler->last.reg = reg;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
}

static void compile_object(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    uint8_t reg = compiler->regIndex - 1;

    emit_byte(compiler, OP_OBJ);
    emit_byte(compiler, reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->parser->current.type != TOKEN_RIGHT_BRACE) {
        while (1) {
            uint8_t dest;
            bool isConst;

            switch (compiler->parser->current.type) {
                case TOKEN_IDENTIFIER: {
                    Token prop = compiler->parser->current;
                    parser_advance(compiler->parser);

                    if(prop.size == 6 && memcmp(prop.start, "length", 6) == 0) {
                        parser_error_at_previous(compiler->parser, "The property 'length' is reserved");
                        return;
                    }

                    uint16_t propIndex = create_identifier_constant(compiler);

                    if(propIndex < UINT8_MAX) {
                        isConst = true;

                        dest = propIndex;
                    } else {
                        isConst = false;

                        if (!register_reserve(compiler))
                            return;

                        emit_byte(compiler, OP_CNSTW);
                        emit_byte(compiler, compiler->regIndex - 1);
                        emit_word(compiler, propIndex);

                        dest = compiler->regIndex - 1;
                    }

                    break;
                }
                case TOKEN_STRING: {
                    Token prop = compiler->parser->current;
                    parser_advance(compiler->parser);

                    if(prop.size == 8 && memcmp(prop.start, "\"length\"", 8) == 0) {
                        parser_error_at_previous(compiler->parser, "The property 'length' is reserved");
                        return;
                    }

                    size_t chunkSize = compiler->function->chunk.size;

                    compile_string(compiler, allowAssignment);

                    if(compiler->function->chunk.size != chunkSize) {
                        dest = compiler->function->chunk.bytecode[compiler->function->chunk.size - 4 + 2];
                        compiler->function->chunk.size = chunkSize;
                    } else dest = compiler->last.reg;

                    isConst = compiler->last.isConst;

                    break;
                }
                default:
                    parser_error_at_current(compiler->parser, "Expected identifier or string");
                    return;
            }

            parser_consume(compiler->parser, TOKEN_COLON, "Expected ':' after object key");

            compile_expression_precedence(compiler, PREC_COMMA + 1);

            if(compiler->last.isNew)
                register_free(compiler);

            if(compiler->last.isConst) {
                compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 4 + 2];
                compiler->function->chunk.size -= 4;
            }

            #define LR_TYPES ((isConst * 0x80) | (compiler->last.isConst * 0x40))

            emit_byte(compiler, OP_SET | LR_TYPES);
            emit_byte(compiler, reg);
            emit_byte(compiler, dest);
            emit_byte(compiler, compiler->last.reg);

            #undef LR_TYPES

            if(compiler->parser->current.type == TOKEN_RIGHT_BRACE || compiler->parser->current.type == TOKEN_EOF)
                break;

            parser_consume(compiler->parser, TOKEN_COMMA, "Expected ',' after object entry");
        }
    }

    parser_consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expected '}' after object properties");

    compiler->last.reg = reg;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
}

static void compile_object_entries(Compiler* compiler, bool allowAssignment) {

}

static void compile_declaration(Compiler* compiler) {
    size_t regIndex = compiler->regIndex;
    size_t localCount = compiler->localCount;

    if(compiler->parser->current.type == TOKEN_VAR) {
        parser_advance(compiler->parser);
        compile_variable_declaration(compiler);
    } else if(compiler->parser->current.type == TOKEN_FUNCTION) {
        parser_advance(compiler->parser);
        compile_function_declaration(compiler);
    } else compile_statement(compiler);

    if(compiler->parser->panic)
        parser_sync(compiler->parser);

    if(compiler->regIndex - regIndex != compiler->localCount - localCount)
        compiler->regIndex = regIndex + (compiler->localCount - localCount);
}

static void compile_variable_declaration(Compiler* compiler) {
    uint16_t index = declare_variable(compiler);
    uint32_t chunkSize = compiler->function->chunk.size;
    Token lastRegToken = compiler->parser->previous;

    if(compiler->parser->current.type == TOKEN_EQUAL) {
        parser_advance(compiler->parser);
        compile_expression(compiler);
    } else {
        if(!register_reserve(compiler))
            return;

        emit_byte(compiler, OP_NULL);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
    }

    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration");

    if(compiler->scopeDepth > 0) {
        Chunk chunk = compiler->function->chunk;

        if((chunkSize == chunk.size)                  // Register reference; no new OPs.
        || (chunkSize + 4 == chunk.size               // Only one OP.
          && (chunk.bytecode[chunkSize] == OP_INC     // OP is INC.
          || (chunk.bytecode[chunkSize] == OP_DEC)))) // OP is DEC.
            emit_mov(compiler, compiler->localCount - 1, compiler->last.reg); // MOV the origin.

        else if(compiler->last.isPostIncrement) {
            int32_t incOffset = 4;

            while(chunk.size - incOffset >= chunkSize && chunk.bytecode[chunk.size - incOffset] != OP_INC && chunk.bytecode[chunk.size - incOffset] != OP_DEC)
                incOffset += 4;

            if(chunk.size - incOffset < chunkSize) {
                parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix when it isn't (report this to the developers)");
                return;
            }

            if(chunk.size - incOffset - 4 < chunkSize || chunk.bytecode[chunk.size - incOffset - 4] != OP_MOV) {
                parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not MOV (report this to the developers)");
                return;
            }

            if(chunk.bytecode[chunk.size - incOffset + 1] == compiler->localCount - 1) { // The INC target interferes with the local register.
                uint8_t dest = compiler->localCount - 1;
                uint8_t tmp = chunk.bytecode[chunk.size - incOffset - 4 + 1]; // Original MOV target.

                chunk.bytecode[chunk.size - incOffset - 4 + 2] = tmp;  // MOV source.
                chunk.bytecode[chunk.size - incOffset - 4 + 1] = dest; // MOV target.
                chunk.bytecode[chunk.size - incOffset + 1] = tmp;      // INC target.

                incOffset += 8; // Jump over MOV.

                if(chunk.size - incOffset >= chunkSize) {
                    if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_GET) {
                        parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not GET (report this to the developers)");
                        return;
                    }

                    chunk.bytecode[chunk.size - incOffset + 1] = tmp;
                }

                incOffset -= 4 + 8;

                if(incOffset > 0) {
                    if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_SET) {
                        parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC successor is not SET (report this to the developers)");
                        return;
                    }

                    chunk.bytecode[chunk.size - incOffset + 3] = tmp;
                }

                incOffset -= 4;

                if(incOffset > 0) {
                    if((chunk.bytecode[chunk.size - incOffset] & INSTRUCTION_MASK) != OP_SGLOB) {
                        parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but SET successor is not SGLOB (report this to the developers)");
                        return;
                    }

                    chunk.bytecode[chunk.size - incOffset + 2] = tmp;
                }
            } else chunk.bytecode[chunk.size - incOffset - 4 + 1] = compiler->localCount - 1; // Directly MOV to local.
        } else if(op_has_direct_dest(chunk.bytecode[chunk.size - 4] & INSTRUCTION_MASK)&& !compiler->last.isEqualOp) { // Can directly assign to local.
            chunk.bytecode[chunk.size - 4 + 1] = compiler->localCount - 1;  // Do it.
            compiler->last.reg = index;
        } else emit_mov(compiler, compiler->localCount - 1, compiler->last.reg); // MOV the result.

        if(compiler->last.isNew)
            register_free(compiler);

        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;

        compiler->last.reg = compiler->localCount - 1;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_LOCAL, lastRegToken };
        compiler->last.isNew = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;

        if(compiler->regIndex == 249) {
            parser_error_at_current(compiler->parser, "Register limit exceeded (250)");
            return;
        }

        ++compiler->regIndex;
        return;
    }

    if(compiler->last.isNew)
        register_free(compiler);

    Chunk* chunk = &compiler->function->chunk;
    if(chunk->bytecode[chunk->size - 4] == OP_CNST) {
        compiler->last.reg = chunk->bytecode[chunk->size - 2];
        compiler->last.isConst = true;
        chunk->size -= 4;
    }

    #define L_TYPE (compiler->last.isConst * 0x80)

    emit_byte(compiler, OP_DGLOB | L_TYPE);
    emit_byte(compiler, index);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);

    #undef L_TYPE

    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
}

static void compile_function_declaration(Compiler* compiler) {
    uint16_t index = declare_variable(compiler);

    if(!register_reserve(compiler))
        return;

    if(compiler->scopeDepth > 0)
        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;

    compile_function(compiler);

    if(compiler->scopeDepth > 0)
        return;

    Chunk* chunk = &compiler->function->chunk;
    if(chunk->bytecode[chunk->size - 4] == OP_CNST) {
        compiler->last.reg = chunk->bytecode[chunk->size - 2];
        compiler->last.isConst = true;
        chunk->size -= 4;
    } else compiler->last.reg = compiler->regIndex - 1;

    register_free(compiler);

    #define L_TYPE (compiler->last.isConst * 0x80)

    emit_byte(compiler, OP_DGLOB | L_TYPE);
    emit_byte(compiler, index);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);

    #undef L_TYPE

    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
}

static void compile_function(Compiler* compiler) {
    Compiler subcompiler;
    compiler_init(&subcompiler);

    const char* start = compiler->parser->previous.start;
    uint32_t length = compiler->parser->previous.size;
    uint32_t hash = map_hash(start, length);

    Compiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.super = compiler;
    subcompiler.parser = compiler->parser;

    scope_begin(&subcompiler);
    parser_consume(subcompiler.parser, TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if(subcompiler.parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            ++subcompiler.function->arity;

            if (subcompiler.function->arity > 250) {
                parser_error_at_current(subcompiler.parser, "Parameter limit exceeded (250)");
                return;
            }

            declare_variable(&subcompiler);

            subcompiler.locals[subcompiler.localCount - 1].depth = subcompiler.scopeDepth;

            ++subcompiler.regIndex;
        } while(subcompiler.parser->current.type == TOKEN_COMMA && (parser_advance(subcompiler.parser), true));
    }

    parser_consume(subcompiler.parser, TOKEN_RIGHT_PAREN, "Expected ')' after parameters");

    if(subcompiler.parser->current.type == TOKEN_EQUAL_GREATER) {
        parser_advance(subcompiler.parser);
        compile_return_statement(&subcompiler);
    } else {
        parser_consume(subcompiler.parser, TOKEN_LEFT_BRACE, "Expected '{' before function body");
        compile_block(&subcompiler);
        emit_return(&subcompiler);
    }

    if(compiler->scopeDepth == 0)
        if(!register_reserve(compiler))
            return;

    if(subcompiler.upvalueCount == 0) {
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));
    } else {
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));

        emit_byte(compiler, OP_CLSR);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, subcompiler.upvalueCount);

        for(uint8_t i = 0; i < subcompiler.upvalueCount; ++i) {
            emit_byte(compiler, OP_UPVAL);
            emit_byte(compiler, subcompiler.upvalues[i].index);
            emit_byte(compiler, subcompiler.upvalues[i].local);
            emit_byte(compiler, 0);
        }
    }

    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    compiler_delete(&subcompiler);
}

static void compile_statement(Compiler* compiler) {
    switch(compiler->parser->current.type) {
        case TOKEN_IF:
            parser_advance(compiler->parser);
            compile_if_statement(compiler);
            break;
        case TOKEN_WHILE:
            parser_advance(compiler->parser);
            compile_while_statement(compiler);
            break;
        case TOKEN_FOR:
            parser_advance(compiler->parser);
            compile_for_statement(compiler);
            break;
        case TOKEN_RETURN:
            parser_advance(compiler->parser);
            compile_return_statement(compiler);
            break;
        case TOKEN_CONTINUE:
            parser_advance(compiler->parser);
            compile_continue_statement(compiler);
            break;
        case TOKEN_BREAK:
            parser_advance(compiler->parser);
            compile_break_statement(compiler);
            break;
        case TOKEN_LEFT_BRACE:
            parser_advance(compiler->parser);

            scope_begin(compiler);
            compile_block(compiler);
            scope_end(compiler);
            break;
        case TOKEN_DOLLAR:
            parser_advance(compiler->parser);
            compile_inline_asm_statement(compiler);
            break;
        case TOKEN_PERCENT:
            parser_advance(compiler->parser);
            compile_disasm_statement(compiler);
            break;
        case TOKEN_SEMICOLON:
            parser_advance(compiler->parser);
            break;
        default:
            compile_expression_statement(compiler);
            break;
    }
}

static void compile_if_statement(Compiler* compiler) {
    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->last.isNew)
        register_free(compiler);

    uint32_t ifEnd = emit_blank(compiler);

    compile_statement(compiler);

    if(compiler->parser->current.type == TOKEN_ELSE) {
        uint32_t elseEnd = emit_blank(compiler);

        emit_jump(compiler, ifEnd);
        parser_advance(compiler->parser);
        compile_statement(compiler);
        emit_jump(compiler, elseEnd);
    } else emit_jump(compiler, ifEnd);
}

static void compile_while_statement(Compiler* compiler) {
    uint32_t start = compiler->function->chunk.size;

    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->last.isNew)
        register_free(compiler);

    uint32_t end = emit_blank(compiler);

    if(compiler->loopCount == 250) {
        parser_error_at_previous(compiler->parser, "Loop limit exceeded (250)");
        return;
    }

    ++compiler->loopCount;

    for(uint8_t i = 0; i < compiler->leapCount; ++i)
        ++compiler->leaps[i].depth;

    compile_statement(compiler);

    emit_backwards_jump(compiler, start);
    emit_jump(compiler, end);

    for(uint8_t i = 0; i < compiler->leapCount; ++i) {
        Leap* leap = &compiler->leaps[i];
        --leap->depth;

        if(leap->depth == 0) {
            if(leap->isBreak)
                emit_jump(compiler, leap->index);
            else emit_backwards_jump_from(compiler, leap->index, start);
        }
    }
}

static void compile_for_statement(Compiler* compiler) {
    scope_begin(compiler);

    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'for'");

    if(compiler->parser->current.type == TOKEN_SEMICOLON) {
        parser_advance(compiler->parser);
    } else if(compiler->parser->current.type == TOKEN_VAR) {
        parser_advance(compiler->parser);
        compile_variable_declaration(compiler);
    } else {
        parser_advance(compiler->parser);
        compile_expression_statement(compiler);
    }

    uint32_t start = compiler->function->chunk.size;
    uint32_t exitIndex = 0;
    bool infinite = true;

    if(compiler->parser->current.type != TOKEN_SEMICOLON) {
        compile_expression(compiler);
        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after loop condition");

        emit_byte(compiler, OP_TEST);
        emit_byte(compiler, compiler->last.reg);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        if(compiler->last.isNew)
            register_free(compiler);

        exitIndex = emit_blank(compiler);
        infinite = false;
    }

    if(compiler->parser->current.type != TOKEN_RIGHT_PAREN) {
        uint32_t bodyJump = emit_blank(compiler);
        uint32_t post = compiler->function->chunk.size;
        uint8_t regIndex = compiler->regIndex;

        compile_expression(compiler);

        if(regIndex != compiler->regIndex)
            register_free(compiler);

        parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after clauses");

        emit_backwards_jump(compiler, start);
        start = post;
        emit_jump(compiler, bodyJump);
    }

    if(compiler->loopCount == 250) {
        parser_error_at_previous(compiler->parser, "Loop limit exceeded (250)");
        return;
    }

    ++compiler->loopCount;

    for(uint8_t i = 0; i < compiler->leapCount; ++i)
        ++compiler->leaps[i].depth;

    compile_statement(compiler);
    emit_backwards_jump(compiler, start);

    if(!infinite) {
        emit_jump(compiler, exitIndex);
    }

    for(uint8_t i = 0; i < compiler->leapCount; ++i) {
        Leap* leap = &compiler->leaps[i];
        --leap->depth;

        if(leap->depth == 0) {
            if(leap->isBreak)
                emit_jump(compiler, leap->index);
            else emit_backwards_jump_from(compiler, leap->index, start);
        }
    }

    scope_end(compiler);
}

static void compile_return_statement(Compiler* compiler) {
    if(compiler->function->name == NULL) {
        parser_error_at_previous(compiler->parser, "Cannot return from top-level scope");
    }

    if(compiler->parser->current.type == TOKEN_SEMICOLON) {
        parser_advance(compiler->parser);
        emit_return(compiler);
    } else {
        compile_expression(compiler);
        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after return expression");

        emit_byte(compiler, OP_RET);
        emit_byte(compiler, compiler->last.reg);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        if(compiler->last.isNew)
            register_free(compiler);
    }
}

static void compile_return_expression(Compiler* compiler) {
    if(compiler->function->name == NULL) {
        parser_error_at_previous(compiler->parser, "Cannot return from top-level scope");
    }

    if(compiler->parser->current.type == TOKEN_SEMICOLON) {
        emit_return(compiler);
    } else {
        compile_expression_precedence(compiler, PREC_COMMA + 1);

        emit_byte(compiler, OP_RET);
        emit_byte(compiler, compiler->last.reg);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        if(compiler->last.isNew)
            register_free(compiler);
    }
}

static void compile_continue_statement(Compiler* compiler) {
    if(compiler->loopCount == 0) {
        parser_error_at_previous(compiler->parser, "Cannot continue outside of loops");
        return;
    }
    if(compiler->leapCount == 250) {
        parser_error_at_previous(compiler->parser, "Leap limit exceeded (250)");
        return;
    }

    Leap leap;
    leap.isBreak = false;
    leap.index = compiler->function->chunk.size;

    if(compiler->parser->current.type == TOKEN_SEMICOLON) {
        parser_advance(compiler->parser);
        leap.depth = 1;
    } else if(compiler->parser->current.type == TOKEN_INT) {
        parser_advance(compiler->parser);

        int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

        if(errno == ERANGE) {
            parser_error_at_previous(compiler->parser, "Number is too large for type 'int'");
            return;
        }
        if(num < 0) {
            parser_error_at_previous(compiler->parser, "Continue depth cannot be negative");
            return;
        }
        if(num > compiler->loopCount) {
            parser_error_at_previous(compiler->parser, "Cannot continue from that many loops; consider using 'continue 0;'");
            return;
        }

        if(num == 0)
            leap.depth = compiler->loopCount;
        else leap.depth = (uint8_t) num;

        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after continue statement");
    } else {
        parser_error_at_previous(compiler->parser, "Expected ';' or number after 'continue'");
        return;
    }

    compiler->leaps[compiler->leapCount++] = leap;
    emit_blank(compiler);
}

static void compile_break_statement(Compiler* compiler) {
    if(compiler->loopCount == 0) {
        parser_error_at_previous(compiler->parser, "Cannot break outside of loops");
        return;
    }
    if(compiler->leapCount == 250) {
        parser_error_at_previous(compiler->parser, "Leap limit exceeded (250)");
        return;
    }

    Leap leap;
    leap.isBreak = true;
    leap.index = compiler->function->chunk.size;

    if(compiler->parser->current.type == TOKEN_SEMICOLON) {
        parser_advance(compiler->parser);
        leap.depth = 1;
    } else if(compiler->parser->current.type == TOKEN_INT) {
        parser_advance(compiler->parser);

        int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

        if(errno == ERANGE) {
            parser_error_at_previous(compiler->parser, "Number is too large for type 'int'");
            return;
        }
        if(num < 0) {
            parser_error_at_previous(compiler->parser, "Break depth cannot be negative");
            return;
        }
        if(num > compiler->loopCount) {
            parser_error_at_previous(compiler->parser, "Cannot break from that many loops; consider using 'break 0;'");
            return;
        }

        if(num == 0)
            leap.depth = compiler->loopCount;
        else leap.depth = (uint8_t) num;

        parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after break statement");
    } else {
        parser_error_at_previous(compiler->parser, "Expected ';' or number after 'break'");
        return;
    }

    compiler->leaps[compiler->leapCount++] = leap;
    emit_blank(compiler);
}

static void compile_block(Compiler* compiler) {
    while(compiler->parser->current.type != TOKEN_EOF && compiler->parser->current.type != TOKEN_RIGHT_BRACE) {
        compile_declaration(compiler);
    }

    parser_consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void compile_inline_asm_statement(Compiler* compiler) {
    bool isBlock = false;

    if(compiler->parser->current.type == TOKEN_LEFT_BRACE) {
        isBlock = true;
        parser_advance(compiler->parser);
    }

    Assembler iasm;

    assembler_init(&iasm);

    Compiler* super = compiler->super != NULL ? compiler->super : compiler;

    while(super->super != NULL)
        super = super->super;

    iasm.chunk = compiler->function->chunk;
    iasm.strings = &super->strings;

    assembler_assemble(&iasm, compiler->parser->lexer.start, isBlock ? "}" : "\r\n");

    compiler->function->chunk = iasm.chunk;
    compiler->function->chunk.constants = iasm.chunk.constants;

    compiler->parser->lexer.start = iasm.parser->lexer.start;
    compiler->parser->lexer.current = iasm.parser->lexer.current;
    compiler->parser->lexer.index += iasm.parser->lexer.index - 1;

    if(iasm.parser->error)
        compiler->parser->error = true;

    parser_advance(compiler->parser);

    assembler_delete(&iasm);

    if(isBlock)
        parser_consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expected '}' after inline asm statement");
}

static void compile_disasm_statement(Compiler* compiler) {
    bool isSelf = false;

    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after '%'");

    if(compiler->parser->current.type == TOKEN_RIGHT_PAREN)
        isSelf = true;
    else compile_expression(compiler);

    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after argument");
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after ')'");

    if(compiler->last.isNew)
        register_free(compiler);

    emit_byte(compiler, OP_DIS);
    emit_byte(compiler, isSelf ? 251 : compiler->last.reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);
}

static void compile_expression_statement(Compiler* compiler) {
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after expression");
}

static void compile_expression(Compiler* compiler) {
    compile_expression_precedence(compiler, PREC_COMMA);
}

static void compile_expression_precedence(Compiler* compiler, Precedence precedence) {
    parser_advance(compiler->parser);

    RuleHandler prefix = EXPRESSION_RULES[compiler->parser->previous.type].prefix;

    if(prefix == NULL) {
        parser_error_at_previous(compiler->parser, "Expected expression");
        return;
    }

    bool allowAssignment = precedence <= PREC_ASSIGNMENT;
    prefix(compiler, allowAssignment);

    while(precedence <= EXPRESSION_RULES[compiler->parser->current.type].precedence) {
        parser_advance(compiler->parser);

        RuleHandler infix = EXPRESSION_RULES[compiler->parser->previous.type].inpostfix;
        infix(compiler, allowAssignment);
    }

    if(allowAssignment && (compiler->parser->current.type == TOKEN_EQUAL)) {
        parser_error_at_previous(compiler->parser, "Invalid assignment target");
        return;
    }
}

static void compile_call(Compiler* compiler, bool allowAssignment) {
    if(!compiler->last.isNew) {
        if(!register_reserve(compiler))
            return;
        emit_mov(compiler, compiler->regIndex - 1,  compiler->last.reg);
        compiler->last.reg = compiler->regIndex - 1;
    }

    uint8_t functionReg = compiler->last.reg;

    if(compiler->regIndex <= functionReg)
        if(!register_reserve(compiler))
            return;

    compiler->last.canOverwrite = true;

    uint8_t argc = compile_arguments(compiler);

    compiler->last.canOverwrite = false;

    emit_byte(compiler, OP_CALL);
    emit_byte(compiler, functionReg);
    emit_byte(compiler, argc);
    emit_byte(compiler, 0);

    while(argc > 0) {
        register_free(compiler);
        --argc;
    }

    compiler->last.reg = functionReg;
    compiler->regs[functionReg] = (RegInfo) { REG_TEMP };
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
}

static void compile_clone(Compiler* compiler, bool allowAssignment) {
    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'clone' keyword");
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after clone argument");

    uint8_t destReg;

    if(compiler->last.isNew) {
        destReg = compiler->last.reg;

        register_free(compiler);
    } else {
        if(!register_reserve(compiler))
            return;
        destReg = compiler->regIndex - 1;
    }

    emit_byte(compiler, OP_CLONE);
    emit_byte(compiler, destReg);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);

    compiler->last.reg = destReg;
    compiler->regs[destReg] = (RegInfo) { REG_TEMP };
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
}

static void compile_dot(Compiler* compiler, bool allowAssignment) {
    uint8_t leftReg = compiler->last.reg;
    bool leftNew = compiler->last.isNew;

    if(compiler->parser->current.type != TOKEN_IDENTIFIER) {
        parser_error_at_current(compiler->parser, "Expected identifier");
        return;
    }

    Token prop = compiler->parser->current;
    parser_advance(compiler->parser);

    uint16_t propIndex = create_identifier_constant(compiler);
    bool identifierConst;

    if(propIndex < UINT8_MAX) {
        identifierConst = true;

        compiler->last.reg = propIndex;
        compiler->last.isNew = false;
    } else {
        identifierConst = false;

        if(!register_reserve(compiler))
            return;

        emit_byte(compiler, OP_CNSTW);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_word(compiler, propIndex);

        compiler->last.reg = compiler->regIndex -1;
        compiler->last.isNew = true;
    }

    if(allowAssignment && (compiler->parser->current.type == TOKEN_EQUAL)) {
        if(prop.size == 6 && memcmp(prop.start, "length", 6) == 0) {
            parser_error_at_previous(compiler->parser, "Cannot assign to length");
            return;
        }

        uint8_t rightReg = compiler->last.reg;
        bool rightNew = compiler->last.isNew;

        parser_advance(compiler->parser);
        compile_expression(compiler);

        #define L_TYPE (identifierConst * 0x80)

        emit_byte(compiler, OP_SET | L_TYPE);
        emit_byte(compiler, leftReg);
        emit_byte(compiler, rightReg);
        emit_byte(compiler, compiler->last.reg);

        #undef L_TYPE

        if(leftNew)
            register_free(compiler);
        if(rightNew)
            register_free(compiler);
        if(!compiler->last.canOverwrite || compiler->last.lvalMeta.type == LVAL_GLOBAL)
            register_free(compiler);

        compiler->last.isLvalue = false;
        compiler->last.isConst = false; // CNST was already handled here.
    } else {
        uint8_t destReg;

        if(compiler->last.isNew) {
            destReg = compiler->last.reg;

            if(leftNew)
                register_free(compiler);
        } else if(compiler->last.canOverwrite && leftNew) {
            destReg = leftReg;
        } else {
            register_reserve(compiler);
            destReg = compiler->regIndex - 1;
        }

        if(prop.size == 6 && memcmp(prop.start, "length", 6) == 0) {
            emit_byte(compiler, OP_LEN);
            emit_byte(compiler, destReg);
            emit_byte(compiler, leftReg);
            emit_byte(compiler, 0);

            compiler->last.isLvalue = false;
        } else {
            #define R_TYPE (identifierConst * 0x40)

            emit_byte(compiler, OP_GET | R_TYPE);
            emit_byte(compiler, destReg);
            emit_byte(compiler, leftReg);
            emit_byte(compiler, compiler->last.reg);

            #undef R_TYPE
        }

        switch(compiler->last.lvalMeta.type) {
            case LVAL_LOCAL:
                compiler->last.lvalMeta.type = LVAL_LOCAL_PROP;
                break;
            case LVAL_GLOBAL:
                compiler->last.lvalMeta.type = LVAL_GLOBAL_PROP;
                break;
            case LVAL_UPVAL:
                compiler->last.lvalMeta.type = LVAL_UPVAL_PROP;
                break;
            default:
                break;
        }

        compiler->last.lvalMeta.propOrigin = leftReg;
        compiler->last.lvalMeta.propIndex.isConst = compiler->last.isConst;

        if(compiler->last.isConst)
            compiler->last.lvalMeta.propIndex.as.cnst = compiler->last.reg;
        else compiler->last.lvalMeta.propIndex.as.reg = compiler->last.reg;

        compiler->last.reg = destReg;
        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = true;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
    }
}

static uint8_t compile_arguments(Compiler* compiler) {
    uint8_t argc = 0;

    if(compiler->parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            uint32_t chunkSize = compiler->function->chunk.size;
            uint8_t regIndex = compiler->regIndex;

            compile_expression_precedence(compiler, PREC_COMMA + 1);

            if(regIndex != compiler->regIndex)
                compiler->regIndex = regIndex;

            if(!register_reserve(compiler))
                return 255;

            if(chunkSize == compiler->function->chunk.size)
                emit_mov(compiler, compiler->regIndex - 1, compiler->last.reg);
            else if(op_has_direct_dest(compiler->function->chunk.bytecode[compiler->function->chunk.size - 4] & INSTRUCTION_MASK) && !compiler->last.isEqualOp)
                compiler->function->chunk.bytecode[compiler->function->chunk.size - 3] = compiler->regIndex - 1;
            else emit_mov(compiler, compiler->regIndex - 1, compiler->last.reg);

            if(argc == 255) {
                parser_error_at_previous(compiler->parser, "Argument limit exceeded (255)");
                return 255;
            }

            ++argc;
        } while(compiler->parser->current.type == TOKEN_COMMA && (parser_advance(compiler->parser), true));
    }

    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argc;
}

static void compile_grouping_or_lambda(Compiler* compiler, bool allowAssignment) {
    uint32_t backupSize = compiler->function->chunk.size;
    Parser backupParser = *compiler->parser;

    if(compiler->parser->current.type != TOKEN_RIGHT_PAREN) {
        compile_expression(compiler);
        parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
    } else {
        if(parser_advance(compiler->parser), compiler->parser->current.type != TOKEN_EQUAL_GREATER) {
            parser_error_at_previous(compiler->parser, "Unexpected empty parentheses group");
            return;
        }

        compiler->function->chunk.size = backupSize;
        memcpy(compiler->parser, &backupParser, sizeof(Parser));

        compile_lambda(compiler);
        return;
    }

    // Lambda.
    if(compiler->parser->current.type == TOKEN_EQUAL_GREATER) {
        if(compiler->last.isNew)
            register_free(compiler);

        compiler->function->chunk.size = backupSize;
        memcpy(compiler->parser, &backupParser, sizeof(Parser));

        compile_lambda(compiler);
    }
}

static void compile_lambda(Compiler* compiler) {
    Compiler subcompiler;
    compiler_init(&subcompiler);

    const char* start = "lambda";
    uint32_t length = 6;
    uint32_t hash = map_hash(start, length);

    Compiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.super = compiler;
    subcompiler.parser = compiler->parser;

    scope_begin(&subcompiler);

    if(subcompiler.parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            ++subcompiler.function->arity;

            if (subcompiler.function->arity > 250) {
                parser_error_at_current(subcompiler.parser, "Parameter limit exceeded (250)");
            }

            declare_variable(&subcompiler);
            subcompiler.locals[subcompiler.localCount - 1].depth = subcompiler.scopeDepth;

            //++subcompiler.regIndex;
        } while(subcompiler.parser->current.type == TOKEN_COMMA && (parser_advance(subcompiler.parser), true));
    }

    parser_consume(subcompiler.parser, TOKEN_RIGHT_PAREN, "Expected ')' after lambda parameters");
    parser_consume(subcompiler.parser, TOKEN_EQUAL_GREATER, "Expected '=>' after lambda parameters");

    if(subcompiler.parser->current.type == TOKEN_LEFT_BRACE) {
        parser_advance(subcompiler.parser);
        compile_block(&subcompiler);
        emit_return(&subcompiler);
    } else compile_return_expression(&subcompiler);

    if(!register_reserve(compiler))
        return;

    if(subcompiler.upvalueCount == 0) {
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));
    } else {
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));

        emit_byte(compiler, OP_CLSR);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, subcompiler.upvalueCount);

        for(uint8_t i = 0; i < subcompiler.upvalueCount; ++i) {
            emit_byte(compiler, OP_UPVAL);
            emit_byte(compiler, subcompiler.upvalues[i].index);
            emit_byte(compiler, subcompiler.upvalues[i].local);
            emit_byte(compiler, 0);
        }
    }

    compiler->last.reg = compiler->regIndex - 1;
    compiler->regs[compiler->last.reg] = (RegInfo) { REG_CONSTANT, { TOKEN_IDENTIFIER, "lambda", 6 } };
    compiler->last.isNew = true;
    compiler->last.isConst = true;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    compiler_delete(&subcompiler);
}

static void compile_accessor(Compiler* compiler, bool allowAssignment) {
    uint8_t leftReg = compiler->last.reg;
    bool isLeftNew = compiler->last.isNew;
    bool isLeftConst = compiler->last.isConst;

    compile_expression(compiler);

    bool isLength = (compiler->parser->previous.type == TOKEN_STRING && compiler->parser->previous.size == 8 && memcmp(compiler->parser->previous.start, "\"length\"", 8) == 0);

    parser_consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expected ']' after expression");

    if(compiler->last.isConst) {
        register_free(compiler);
        compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 2];
        compiler->function->chunk.size -= 4;
        compiler->last.isNew = false;
    }

    if(allowAssignment && (compiler->parser->current.type == TOKEN_EQUAL)) {
        if(isLength) {
            parser_error_at_previous(compiler->parser, "Cannot assign to length");
            return;
        }

        uint8_t rightReg = compiler->last.reg;
        bool isRightConst = compiler->last.isConst;
        bool isRightNew = compiler->last.isNew;

        parser_advance(compiler->parser);
        compile_expression(compiler);

        #define L_TYPE (isRightConst * 0x80)

        emit_byte(compiler, OP_SET | L_TYPE);
        emit_byte(compiler, leftReg);
        emit_byte(compiler, rightReg);
        emit_byte(compiler, compiler->last.reg);

        #undef L_TYPE

        if(isLeftNew)
            register_free(compiler);
        if(isRightNew)
            register_free(compiler);
        if(!compiler->last.canOverwrite || compiler->last.lvalMeta.type == LVAL_GLOBAL)
            register_free(compiler);

        compiler->last.reg = leftReg;
        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
    } else {
        uint8_t destReg;

        if(compiler->last.isNew) {
            destReg = compiler->last.reg;

            if(isLeftNew)
                register_free(compiler);
        } else if(compiler->last.canOverwrite && isLeftNew) {
            destReg = leftReg;
        } else {
            register_reserve(compiler);
            destReg = compiler->regIndex - 1;
        }

        #define R_TYPE (compiler->last.isConst * 0x40)

        emit_byte(compiler, isLength ? OP_LEN : OP_GET | R_TYPE);
        emit_byte(compiler, destReg);
        emit_byte(compiler, leftReg);
        emit_byte(compiler, isLength ? 0 : compiler->last.reg);

        #undef R_TYPE

        switch(compiler->last.lvalMeta.type) {
            case LVAL_LOCAL:
                compiler->last.lvalMeta.type = LVAL_LOCAL_PROP;
                break;
            case LVAL_GLOBAL:
                compiler->last.lvalMeta.type = LVAL_GLOBAL_PROP;
                break;
            case LVAL_UPVAL:
                compiler->last.lvalMeta.type = LVAL_UPVAL_PROP;
                break;
            default:
                break;
        }

        compiler->last.lvalMeta.propOrigin = leftReg;
        compiler->last.lvalMeta.propIndex.isConst = compiler->last.isConst;

        if(compiler->last.isConst)
            compiler->last.lvalMeta.propIndex.as.cnst = compiler->last.reg;
        else compiler->last.lvalMeta.propIndex.as.reg = compiler->last.reg;

        compiler->last.reg = destReg;
        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = true;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
    }
}

static void compile_unary(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser->previous.type;

    compile_expression_precedence(compiler, PREC_UNARY);

    uint8_t destReg;

    if(compiler->last.isConst) {
        //register_free(compiler);
        destReg = compiler->last.reg;
        compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 2];
        compiler->function->chunk.size -= 4;
    } else if(compiler->last.isNew) {
        destReg = compiler->last.reg;
    } else if(compiler->regs[compiler->last.reg].type != REG_TEMP) {
        if(!register_reserve(compiler))
            return;
        destReg = compiler->regIndex - 1;
    } else destReg = compiler->last.reg;

    #define L_TYPE (compiler->last.isConst * 0x80)

    switch(operator) {
        case TOKEN_BANG:
            emit_bytes(compiler, OP_NOT | L_TYPE, destReg, compiler->last.reg);
            emit_byte(compiler, 0);
            break;
        case TOKEN_TILDE:
            emit_bytes(compiler, OP_BNOT | L_TYPE, destReg, compiler->last.reg);
            emit_byte(compiler, 0);
            break;
        case TOKEN_MINUS:
            emit_bytes(compiler, OP_NEG | L_TYPE, destReg, compiler->last.reg);
            emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    compiler->last.isNew = true;
    compiler->regs[destReg] = (RegInfo) { REG_TEMP };
    compiler->last.reg = destReg;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    #undef L_TYPE
}

static void compile_binary(Compiler* compiler, bool allowAssignment) {
    uint8_t leftReg = compiler->last.reg;
    bool isLeftNew = compiler->last.isNew;
    bool isLeftConst = compiler->last.isConst;

    if(isLeftConst) {
        register_free(compiler);
        leftReg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 2];
        compiler->function->chunk.size -= 4;
        isLeftNew = false;
    }

    TokenType operatorType = compiler->parser->previous.type;

    Rule* rule = &EXPRESSION_RULES[operatorType];
    compile_expression_precedence(compiler, (Precedence) (rule->precedence + 1));

    if(compiler->last.isConst) {
        register_free(compiler);
        compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 2];
        compiler->function->chunk.size -= 4;
        compiler->last.isNew = false;
    }

    // The GT and GTE instructions are simulated with reversed LT and LTE. Therefore, switch the operands and use the REV def.
    #define LR_TYPES ((isLeftConst * 0x80) | (compiler->last.isConst * 0x40))
    #define LR_TYPES_REV ((compiler->last.isConst * 0x80) | (isLeftConst * 0x40))

    switch(operatorType) {
        case TOKEN_PLUS:
            emit_byte(compiler, OP_ADD | LR_TYPES);
            break;
        case TOKEN_MINUS:
            emit_byte(compiler, OP_SUB | LR_TYPES);
            break;
        case TOKEN_STAR:
            emit_byte(compiler, OP_MUL | LR_TYPES);
            break;
        case TOKEN_SLASH:
            emit_byte(compiler, OP_DIV | LR_TYPES);
            break;
        case TOKEN_PERCENT:
            emit_byte(compiler, OP_MOD | LR_TYPES);
            break;
        case TOKEN_LESS_LESS:
            emit_byte(compiler, OP_SHL | LR_TYPES);
            break;
        case TOKEN_GREATER_GREATER:
            emit_byte(compiler, OP_SHR | LR_TYPES);
            break;
        case TOKEN_GREATER:
            emit_byte(compiler, OP_LT | LR_TYPES_REV);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_byte(compiler, OP_LTE | LR_TYPES_REV);
            break;
        case TOKEN_LESS:
            emit_byte(compiler, OP_LT | LR_TYPES);
            break;
        case TOKEN_LESS_EQUAL:
            emit_byte(compiler, OP_LTE | LR_TYPES);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_byte(compiler, OP_EQ | LR_TYPES);
            break;
        case TOKEN_BANG_EQUAL:
            emit_byte(compiler, OP_NEQ | LR_TYPES);
            break;
        case TOKEN_AMPERSAND:
            emit_byte(compiler, OP_BAND | LR_TYPES);
            break;
        case TOKEN_CARET:
            emit_byte(compiler, OP_BXOR | LR_TYPES);
            break;
        case TOKEN_PIPE:
            emit_byte(compiler, OP_BOR | LR_TYPES);
            break;
        default:
            return;
    }

    uint8_t destReg;

    if(compiler->last.isNew/*compiler->regs[compiler->last.reg].type == REG_TEMP*/) {
        destReg = compiler->last.reg;

        if(isLeftNew) { //TODO: check if this works for all cases.
            destReg = leftReg;
            register_free(compiler);
        }
    } else if(isLeftNew/*compiler->regs[leftReg].type == REG_TEMP*/) {
        destReg = leftReg;
    } else {
        register_reserve(compiler);
        destReg = compiler->regIndex - 1;
    }

    // Reverse in the case of < and <= because only LT and LTE exist.
    if(operatorType == TOKEN_GREATER || operatorType == TOKEN_GREATER_EQUAL)
        emit_bytes(compiler, destReg, compiler->last.reg, leftReg);
    else emit_bytes(compiler, destReg, leftReg, compiler->last.reg);

    compiler->regs[destReg] = (RegInfo) { REG_TEMP };
    compiler->last.reg = destReg;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;

    #undef LR_TYPES
}

static void compile_ternary(Compiler* compiler, bool allowAssignment) {
    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->regIndex - 1);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    register_free(compiler);

    uint32_t first = emit_blank(compiler);

    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_COLON, "Expected ':' after ternary operator expression");
    register_free(compiler);

    uint32_t second = emit_blank(compiler);

    emit_jump(compiler, first);
    compile_expression(compiler);
    emit_jump(compiler, second);
}

static void compile_equal_op(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser->previous.type;

    if(!compiler->last.isLvalue) {
        parser_error_at_current(compiler->parser, "Cannot assign to non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    bool isNew = compiler->last.isNew;
    bool isConst = compiler->last.isConst;
    bool isLvalue = compiler->last.isLvalue;

    // Save the lval meta.
    uint8_t meta[sizeof(compiler->last.lvalMeta)];
    memcpy(meta, &compiler->last.lvalMeta, sizeof(compiler->last.lvalMeta));

    compile_expression(compiler);

    if(compiler->last.isConst) {
        compiler->last.reg = compiler->function->chunk.bytecode[compiler->function->chunk.size - 4 + 2];
        compiler->function->chunk.size -= 4;
    }

    #define R_TYPE (compiler->last.isConst * 0x40)

    switch(operator) {
        case TOKEN_PLUS_EQUAL:
            emit_byte(compiler, OP_ADD | R_TYPE);
            break;
        case TOKEN_MINUS_EQUAL:
            emit_byte(compiler, OP_SUB | R_TYPE);
            break;
        case TOKEN_STAR_EQUAL:
            emit_byte(compiler, OP_MUL | R_TYPE);
            break;
        case TOKEN_SLASH_EQUAL:
            emit_byte(compiler, OP_DIV | R_TYPE);
            break;
        case TOKEN_CARET_EQUAL:
            emit_byte(compiler, OP_BXOR | R_TYPE);
            break;
        case TOKEN_PERCENT_EQUAL:
            emit_byte(compiler, OP_MOD | R_TYPE);
            break;
        case TOKEN_PIPE_EQUAL:
            emit_byte(compiler, OP_BOR | R_TYPE);
            break;
        case TOKEN_AMPERSAND_EQUAL:
            emit_byte(compiler, OP_BAND | R_TYPE);
            break;
        default:
            return;
    }

    emit_byte(compiler, destReg);
    emit_byte(compiler, destReg);
    emit_byte(compiler, compiler->last.reg);

    #undef R_TYPE

    // Load the lval meta.
    memcpy(&compiler->last.lvalMeta, meta, sizeof(compiler->last.lvalMeta));

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * 0x80)

    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            emit_byte(compiler, 0);
        case LVAL_UPVAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    compiler->last.reg = destReg;
    compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
    compiler->last.isNew = isNew;
    compiler->last.isConst = isConst;
    compiler->last.isLvalue = isLvalue;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = true;
}

static void compile_prefix(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser->previous.type;

    compiler->last.canOverwrite = false;

    compile_expression_precedence(compiler, PREC_UNARY);

    compiler->last.canOverwrite = false; // true

    if(!compiler->last.isLvalue) {
        parser_error_at_current(compiler->parser, "Cannot increment non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    switch(operator) {
        case TOKEN_MINUS_MINUS:
            emit_bytes(compiler, OP_DEC, destReg, 0);
            emit_byte(compiler, 0);
            break;
        case TOKEN_PLUS_PLUS:
            emit_bytes(compiler, OP_INC, destReg, 0);
            emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * 0x80)

    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            emit_byte(compiler, 0);
        case LVAL_UPVAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    /* Currently, this will give you 13:
     *
     * var a = 5;
     * var b = ++a + a++;
     *
     * If you want to get a more natural response (12), uncomment this line.
     * However, doing so will disable some register_find optimizations.
     * It's not really worth it.
     */
    // compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
}

static void compile_postfix(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser->previous.type;

    if(!compiler->last.isLvalue) {
        parser_error_at_current(compiler->parser, "Cannot increment non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    if(!register_reserve(compiler))
        return;

    emit_bytes(compiler, OP_MOV, compiler->regIndex - 1, destReg);
    emit_byte(compiler, 0);

    switch(operator) {
        case TOKEN_MINUS_MINUS:
            emit_bytes(compiler, OP_DEC, destReg, 0);
            emit_byte(compiler, 0);
            break;
        case TOKEN_PLUS_PLUS:
            emit_bytes(compiler, OP_INC, destReg, 0);
            emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * 0x80)

    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            emit_byte(compiler, 0);
        case LVAL_UPVAL_PROP:
            emit_bytes(compiler, OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                : compiler->last.lvalMeta.propIndex.as.reg);
            emit_byte(compiler, destReg);

            emit_bytes(compiler, OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    compiler->last.reg = compiler->regIndex - 1;
    compiler->regs[compiler->last.reg] = (RegInfo) { REG_TEMP };
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = true;
    compiler->last.isEqualOp = false;
}

static void compile_and(Compiler* compiler, bool allowAssignment) {
    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->last.isNew)
        register_free(compiler);

    uint16_t index = emit_blank(compiler);

    compile_expression_precedence(compiler, PREC_AND);

    emit_jump(compiler, index);
}

static void compile_or(Compiler* compiler, bool allowAssignment) {
    emit_byte(compiler, OP_NTEST);
    emit_byte(compiler, compiler->last.reg);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    if(compiler->last.isNew)
        register_free(compiler);

    uint16_t index = emit_blank(compiler);

    compile_expression_precedence(compiler, PREC_OR);

    emit_jump(compiler, index);
}

static void compile_comma(Compiler* compiler, bool allowAssignment) {
    if(compiler->last.isNew)
        register_free(compiler);

    compile_expression_precedence(compiler, PREC_COMMA);
}

static void scope_begin(Compiler* compiler) {
    ++compiler->scopeDepth;
}

static void scope_end(Compiler* compiler) {
    --compiler->scopeDepth;

    while(compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        if(compiler->locals[compiler->localCount - 1].captured) {
            emit_byte(compiler, OP_CUPVAL);
            emit_byte(compiler, compiler->regIndex - 1);
            emit_byte(compiler, 0);
            emit_byte(compiler, 0);
        }

        register_free(compiler);
        --compiler->localCount;
    }
}

static void local_add(Compiler* compiler, Token identifier) {
    if(compiler->localCount == 250) {
        parser_error_at_previous(compiler->parser, "Local variable limit exceeded (250)");
        return;
    }

    //if(!register_reserve(compiler))
    //    return;

    Local* local = &compiler->locals[compiler->localCount++];
    local->identifier = identifier;
    local->depth = -1;
    local->reg = compiler->regIndex; // -1
    local->captured = false;

    compiler->regs[local->reg] = (RegInfo) { REG_LOCAL, identifier };
}

static uint8_t local_resolve(Compiler* compiler, Token* identifier) {
    for(int16_t i = compiler->localCount - 1; i >= 0; --i) {
        Local* local = &compiler->locals[i];

        if(identifier_equals(identifier, &local->identifier) && local->depth > -1)
            return local->reg;
    }

    return 251;
}

static uint8_t upvalue_add(Compiler* compiler, uint8_t index, bool local) {
    uint8_t upvalCount = compiler->upvalueCount;

    for(uint8_t i = 0; i < upvalCount; ++i) {
        Upvalue* upvalue = &compiler->upvalues[i];

        if(upvalue->index == index && upvalue->local == local)
            return i;
    }

    if(upvalCount == 250) {
        parser_error_at_previous(compiler->parser, "Closure variable limit exceeded (250)");
    }

    compiler->upvalues[upvalCount].local = local;
    compiler->upvalues[upvalCount].index = index;
    return compiler->upvalueCount++;
}

static uint8_t upvalue_resolve(Compiler* compiler, Token* identifier) {
    if(compiler->super == NULL)
        return 251;

    uint8_t local = local_resolve(compiler->super, identifier);

    if(local != 251) {
        compiler->super->locals[local].captured = true;
        return upvalue_add(compiler, local, true);
    }

    uint8_t upvalue = upvalue_resolve(compiler->super, identifier);

    if(upvalue != 251)
        return upvalue_add(compiler, upvalue, false);

    return 251;
}

static void emit_byte(Compiler* compiler, uint8_t byte) {
    chunk_write(&compiler->function->chunk, byte, compiler->parser->previous.index);
}

static void emit_bytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
    emit_byte(compiler, byte1);
    emit_byte(compiler, byte2);
    emit_byte(compiler, byte3);
}

static void emit_word(Compiler* compiler, uint16_t word) {
    emit_byte(compiler, ((uint8_t*) &word)[0]);
    emit_byte(compiler, ((uint8_t*) &word)[1]);
}

static void emit_constant(Compiler* compiler, Value value) {
    uint16_t index = create_constant(compiler, value);

    if(index < UINT8_MAX) {
        emit_bytes(compiler, OP_CNST, compiler->regIndex - 1, index);
        emit_byte(compiler, 0);
    } else {
        emit_byte(compiler, OP_CNSTW);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_word(compiler, index);
    }
}

static void emit_return(Compiler* compiler) {
    Chunk* chunk = &compiler->function->chunk;

    emit_byte(compiler, OP_RET);
    emit_byte(compiler, 251);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);
}

static void emit_mov(Compiler* compiler, uint8_t dest, uint8_t src) {
    if(dest != src) {
        emit_byte(compiler, OP_MOV);
        emit_byte(compiler, dest);
        emit_byte(compiler, src);
        emit_byte(compiler, 0);
    }
}

static uint32_t emit_blank(Compiler* compiler) {
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    return compiler->function->chunk.size - 4;
}

static void emit_jump(Compiler* compiler, uint32_t index) {
    uint32_t diff = (compiler->function->chunk.size - index - 4) / 4;

    if(diff <= UINT8_MAX) {
        compiler->function->chunk.bytecode[index] = OP_JMP;
        compiler->function->chunk.bytecode[index + 1] = (uint8_t) diff;
    } else if(diff <= UINT16_MAX) {
        uint16_t word = (uint16_t) diff;

        compiler->function->chunk.bytecode[index] = OP_JMPW;
        compiler->function->chunk.bytecode[index + 1] = ((uint8_t*) &word)[0];
        compiler->function->chunk.bytecode[index + 2] = ((uint8_t*) &word)[1];
    } else {
        parser_error_at_previous(compiler->parser, "Jump limit exceeded (65535)");
        return;
    }
}

static void emit_backwards_jump(Compiler* compiler, uint32_t to) {
    emit_backwards_jump_from(compiler, compiler->function->chunk.size, to);
}

static void emit_backwards_jump_from(Compiler* compiler, uint32_t from, uint32_t to) {
    uint32_t diff = (from - to) / 4;

    if(diff <= UINT8_MAX) {
        if(from == compiler->function->chunk.size) {
            emit_byte(compiler, OP_BJMP);
            emit_byte(compiler, (uint8_t) diff);
            emit_byte(compiler, 0);
            emit_byte(compiler, 0);
        } else {
            compiler->function->chunk.bytecode[from] = OP_BJMP;
            compiler->function->chunk.bytecode[from + 1] = (uint8_t) diff;
        }
    } else if(diff <= UINT16_MAX) {
        uint16_t word = (uint16_t) diff;

        if(from == compiler->function->chunk.size) {
            emit_byte(compiler, OP_BJMPW);
            emit_word(compiler, word);
            emit_byte(compiler, 0);
        }

        compiler->function->chunk.bytecode[from] = OP_BJMPW;
        compiler->function->chunk.bytecode[from + 1] = ((uint8_t*) &word)[0];
        compiler->function->chunk.bytecode[from + 2] = ((uint8_t*) &word)[1];
    } else {
        parser_error_at_previous(compiler->parser, "Jump limit exceeded (65535)");
        return;
    }
}

static uint16_t create_constant(Compiler* compiler, Value value) {
    size_t index = chunk_write_constant(&compiler->function->chunk, value);

    if(index > UINT16_MAX) {
        parser_error_at_previous(compiler->parser, "Constant limit exceeded (65535)");
        return 0;
    }

    return (uint16_t) index;
}

static uint16_t create_identifier_constant(Compiler* compiler) {
    return create_string_constant(compiler, compiler->parser->previous.start, compiler->parser->previous.size);
}

static uint16_t create_string_constant(Compiler* compiler, const char* start, uint32_t length) {
    uint32_t hash = map_hash(start, length);

    Compiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    return create_constant(compiler, DENSE_VALUE(interned));
}

static uint16_t declare_variable(Compiler* compiler) {
    parser_consume(compiler->parser, TOKEN_IDENTIFIER, "Expected identifier");

    if(compiler->scopeDepth > 0) {
        Token* identifier = &compiler->parser->previous;

        for(int16_t i = compiler->localCount - 1; i >= 0; --i) {
            Local* local = &compiler->locals[i];

            if(local->depth != -1 && local->depth < compiler->scopeDepth)
                break;

            if(identifier_equals(identifier, &local->identifier)) {
                parser_error_at_previous(compiler->parser, "Variable already declared in this scope");
                return -1;
            }
        }

        local_add(compiler, *identifier);
        return 0;
    }

    return create_identifier_constant(compiler);
}

static bool register_reserve(Compiler* compiler) {
    if(compiler->regIndex == 249) {
        parser_error_at_current(compiler->parser, "Register limit exceeded (250)");
        return false;
    }
    else {
        compiler->regs[compiler->regIndex] = (RegInfo) { REG_TEMP };
        ++compiler->regIndex;
        return true;
    }
}

static uint8_t register_find(Compiler* compiler, RegType type, Token token) {
    if(compiler->regIndex == 0)
        return 251;

    for(int16_t i = compiler->regIndex - 1; i >= 0; --i)
        if(compiler->regs[i].type == type)
            if(compiler->regs[i].token.size == token.size && memcmp(compiler->regs[i].token.start, token.start, token.size) == 0)
                return i;
    return 251;
}

static void register_free(Compiler* compiler) {
    --compiler->regIndex;
}

static void finalize_compilation(Compiler* compiler) {
    emit_return(compiler);
}

#undef INSTRUCTION_MASK