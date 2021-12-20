#include "compiler.h"

#include "../cluster/bytecode.h"
#include "../io/log.h"

#include "../asm/assembler.h"
#include "../lib/charlib.h"

#include "../vm/vm.h"

#include <string.h>
#include <stdlib.h>

static void risa_compiler_compile_byte       (RisaCompiler*, bool);
static void risa_compiler_compile_int        (RisaCompiler*, bool);
static void risa_compiler_compile_float      (RisaCompiler*, bool);
static void risa_compiler_compile_string     (RisaCompiler*, bool);
static void risa_compiler_compile_literal    (RisaCompiler*, bool);
static void risa_compiler_compile_identifier (RisaCompiler*, bool);
static void risa_compiler_compile_array      (RisaCompiler*, bool);
static void risa_compiler_compile_object     (RisaCompiler*, bool);

static void risa_compiler_compile_declaration            (RisaCompiler*);
static void risa_compiler_compile_variable_declaration   (RisaCompiler*);
static void risa_compiler_compile_function_declaration   (RisaCompiler*);
static void risa_compiler_compile_function               (RisaCompiler*);
static void risa_compiler_compile_statement              (RisaCompiler*);
static void risa_compiler_compile_if_statement           (RisaCompiler*);
static void risa_compiler_compile_while_statement        (RisaCompiler*);
static void risa_compiler_compile_for_statement          (RisaCompiler*);
static void risa_compiler_compile_return_statement       (RisaCompiler*);
static void risa_compiler_compile_continue_statement     (RisaCompiler*);
static void risa_compiler_compile_break_statement        (RisaCompiler*);
static void risa_compiler_compile_block                  (RisaCompiler*);
static void risa_compiler_compile_inline_asm_statement   (RisaCompiler*);
static void risa_compiler_compile_disasm_statement       (RisaCompiler*);
static void risa_compiler_compile_expression_statement   (RisaCompiler*);
static void risa_compiler_compile_expression             (RisaCompiler*);
static void risa_compiler_compile_expression_precedence  (RisaCompiler*, RisaOperatorPrecedence);
static void risa_compiler_compile_return_expression      (RisaCompiler*);

static void risa_compiler_compile_call                   (RisaCompiler*, bool);
static void risa_compiler_compile_clone                  (RisaCompiler*, bool);
static void risa_compiler_compile_dot                    (RisaCompiler*, bool);
static void risa_compiler_compile_grouping_or_lambda     (RisaCompiler*, bool);
static void risa_compiler_compile_lambda                 (RisaCompiler*);
static void risa_compiler_compile_accessor               (RisaCompiler*, bool);
static void risa_compiler_compile_unary                  (RisaCompiler*, bool);
static void risa_compiler_compile_binary                 (RisaCompiler*, bool);
static void risa_compiler_compile_ternary                (RisaCompiler*, bool);
static void risa_compiler_compile_equal_op               (RisaCompiler*, bool);
static void risa_compiler_compile_prefix                 (RisaCompiler*, bool);
static void risa_compiler_compile_postfix                (RisaCompiler*, bool);
static void risa_compiler_compile_and                    (RisaCompiler*, bool);
static void risa_compiler_compile_or                     (RisaCompiler*, bool);
static void risa_compiler_compile_comma                  (RisaCompiler*, bool);

static uint8_t  risa_compiler_compile_arguments          (RisaCompiler*);

static void     risa_compiler_scope_begin                (RisaCompiler*);
static void     risa_compiler_scope_end                  (RisaCompiler*);

static void     risa_compiler_local_add                  (RisaCompiler*, RisaToken);
static uint8_t  risa_compiler_local_resolve              (RisaCompiler*, RisaToken*);

static uint8_t  risa_compiler_upvalue_add                (RisaCompiler*, uint8_t, bool);
static uint8_t  risa_compiler_upvalue_resolve            (RisaCompiler*, RisaToken*);

static void     risa_compiler_emit_byte                  (RisaCompiler*, uint8_t);
static void     risa_compiler_emit_bytes                 (RisaCompiler*, uint8_t, uint8_t, uint8_t);
static void     risa_compiler_emit_word                  (RisaCompiler*, uint16_t);
static void     risa_compiler_emit_constant              (RisaCompiler*, RisaValue);
static void     risa_compiler_emit_return                (RisaCompiler*);
static void     risa_compiler_emit_mov                   (RisaCompiler*, uint8_t, uint8_t);
static void     risa_compiler_emit_jump                  (RisaCompiler*, uint32_t);
static void     risa_compiler_emit_backwards_jump        (RisaCompiler*, uint32_t);
static void     risa_compiler_emit_backwards_jump_from   (RisaCompiler*, uint32_t, uint32_t);
static uint32_t risa_compiler_emit_blank                 (RisaCompiler*);

static uint16_t risa_compiler_create_constant            (RisaCompiler*, RisaValue);
static uint16_t risa_compiler_create_identifier_constant (RisaCompiler*);
static uint16_t risa_compiler_create_string_constant     (RisaCompiler*, const char*, uint32_t);
static uint16_t risa_compiler_declare_variable           (RisaCompiler*);

static void     risa_compiler_optimize_last_cnst         (RisaCompiler*);
static bool     risa_compiler_can_optimize_last_cnst     (RisaCompiler* compiler);

static bool     risa_compiler_register_reserve           (RisaCompiler*);
static uint8_t  risa_compiler_register_find              (RisaCompiler*, RisaRegType, RisaToken);
static void     risa_compiler_register_free              (RisaCompiler*);

static void risa_compiler_finalize_compilation           (RisaCompiler*);

// Contains all parsing rules for operators.
//
//         rule_for_prefix                              rule_for_infix_or_postfix          operator_precedence           token
const RisaOperatorRule OPERATOR_RULES[] = {
        {  risa_compiler_compile_grouping_or_lambda,    risa_compiler_compile_call,        RISA_PREC_CALL         },  // RISA_TOKEN_LEFT_PAREN
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_RIGHT_PAREN
        {  risa_compiler_compile_array,                 risa_compiler_compile_accessor,    RISA_PREC_CALL         },  // RISA_TOKEN_LEFT_BRACKET
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_RIGHT_BRACKET
        {  risa_compiler_compile_object,                NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_LEFT_BRACE
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_RIGHT_BRACE
        {  NULL,                                        risa_compiler_compile_comma,       RISA_PREC_COMMA        },  // RISA_TOKEN_COMMA
        {  NULL,                                        risa_compiler_compile_dot,         RISA_PREC_CALL         },  // RISA_TOKEN_DOT
        {  risa_compiler_compile_unary,                 risa_compiler_compile_binary,      RISA_PREC_TERM         },  // RISA_TOKEN_MINUS
        {  risa_compiler_compile_prefix,                risa_compiler_compile_postfix,     RISA_PREC_CALL         },  // RISA_TOKEN_MINUS_MINUS
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_TERM         },  // RISA_TOKEN_PLUS
        {  risa_compiler_compile_prefix,                risa_compiler_compile_postfix,     RISA_PREC_CALL         },  // RISA_TOKEN_PLUS_PLUS
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_COLON
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_SEMICOLON
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_FACTOR       },  // RISA_TOKEN_SLASH
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_FACTOR       },  // RISA_TOKEN_STAR
        {  risa_compiler_compile_unary,                 NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_TILDE
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_BITWISE_XOR  },  // RISA_TOKEN_AMPERSAND
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_FACTOR       },  // RISA_TOKEN_PERCENT
        {  NULL,                                        risa_compiler_compile_ternary,     RISA_PREC_TERNARY      },  // RISA_TOKEN_QUESTION
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_DOLLAR
        {  risa_compiler_compile_unary,                 NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_BANG
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_EQUALITY     },  // RISA_TOKEN_BANG_EQUAL
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_EQUAL
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_EQUALITY     },  // RISA_TOKEN_EQUAL_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_EQUAL_GREATER
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_PLUS_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_MINUS_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_STAR_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_SLASH_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_CARET_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_PERCENT_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_PIPE_EQUAL
        {  NULL,                                        risa_compiler_compile_equal_op,    RISA_PREC_ASSIGNMENT   },  // RISA_TOKEN_AMPERSAND_EQUAL
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_COMPARISON   },  // RISA_TOKEN_GREATER
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_COMPARISON   },  // RISA_TOKEN_GREATER_EQUAL
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_SHIFT        },  // RISA_TOKEN_GREATER_GREATER
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_COMPARISON   },  // RISA_TOKEN_LESS
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_COMPARISON   },  // RISA_TOKEN_LESS_EQUAL
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_SHIFT        },  // RISA_TOKEN_LESS_LESS
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_BITWISE_AND  },  // RISA_TOKEN_AMPERSAND
        {  NULL,                                        risa_compiler_compile_and,         RISA_PREC_AND          },  // RISA_TOKEN_AMPERSAND_AMPERSAND
        {  NULL,                                        risa_compiler_compile_binary,      RISA_PREC_BITWISE_OR   },  // RISA_TOKEN_PIPE
        {  NULL,                                        risa_compiler_compile_or,          RISA_PREC_OR           },  // RISA_TOKEN_PIPE_PIPE
        {  risa_compiler_compile_identifier,            NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_IDENTIFIER
        {  risa_compiler_compile_string,                NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_STRING
        {  risa_compiler_compile_byte,                  NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_BYTE
        {  risa_compiler_compile_int,                   NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_INT
        {  risa_compiler_compile_float,                 NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_FLOAT
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_IF
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_ELSE
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_WHILE
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_FOR
        {  risa_compiler_compile_literal,               NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_TRUE
        {  risa_compiler_compile_literal,               NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_FALSE
        {  risa_compiler_compile_literal,               NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_NULL
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_VAR
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_FUNCTION
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_RETURN
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_CONTINUE
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_BREAK
        {  risa_compiler_compile_clone,                 NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_CLONE
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_ERROR
        {  NULL,                                        NULL,                              RISA_PREC_NONE         },  // RISA_TOKEN_EOF
};

RisaCompiler* risa_compiler_create() {
    RisaCompiler* compiler =  RISA_MEM_ALLOC(sizeof(RisaCompiler));

    risa_compiler_init(compiler);

    return compiler;
}

void risa_compiler_init(RisaCompiler* compiler) {
    risa_io_init(&compiler->io);

    compiler->super = NULL;
    compiler->function = risa_dense_function_create();

    risa_cluster_init(&compiler->function->cluster);
    risa_map_init(&compiler->strings);

    compiler->regIndex = 0;
    compiler->options.replMode = false;
    compiler->last.reg = 0;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = false;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.canOverwrite = false; // true
    compiler->last.fromBranched = false;
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

void risa_compiler_target(RisaCompiler* compiler, void* vm) {
    risa_compiler_load_strings(compiler, &((RisaVM*) vm)->strings);

    compiler->options = ((RisaVM*) vm)->options;
}

void risa_compiler_load_strings(RisaCompiler* compiler, RisaMap* strings) {
    risa_map_delete(&compiler->strings);
    compiler->strings = *strings;
}

RisaIO* risa_compiler_get_io(RisaCompiler* compiler) {
    return &compiler->io;
}

RisaDenseFunction* risa_compiler_get_function(RisaCompiler* compiler) {
    return compiler->function;
}

RisaMap* risa_compiler_get_strings(RisaCompiler* compiler) {
    return &compiler->strings;
}

void risa_compiler_set_repl_mode (RisaCompiler* compiler, bool value) {
    compiler->options.replMode = value;
}

void risa_compiler_delete(RisaCompiler* compiler) {
    risa_map_delete(&compiler->strings);
}

void risa_compiler_free(RisaCompiler* compiler) {
    risa_compiler_delete(compiler);
    RISA_MEM_FREE(compiler);
}

void risa_compiler_shallow_free(RisaCompiler* compiler) {
    RISA_MEM_FREE(compiler);
}

RisaCompilerStatus risa_compiler_compile(RisaCompiler* compiler, const char* str) {
    RisaParser parser;
    risa_parser_init(&parser);
    risa_io_clone(&parser.io, &compiler->io); // Use the same IO interface for the parser.

    compiler->parser = &parser;

    risa_lexer_init(&compiler->parser->lexer);
    risa_lexer_source(&compiler->parser->lexer, str);

    risa_parser_advance(compiler->parser);

    while(compiler->parser->current.type != RISA_TOKEN_EOF) {
        risa_compiler_compile_declaration(compiler);

        if(compiler->options.replMode) {
            // ACC is the only instruction that has a type for DEST, and it uses the left type flag for it.
            #define D_TYPE (compiler->last.isConstOptimized * RISA_TODLR_TYPE_LEFT_MASK)

            risa_compiler_emit_byte(compiler, RISA_OP_ACC |  D_TYPE);
            risa_compiler_emit_byte(compiler, compiler->last.reg);
            risa_compiler_emit_byte(compiler, 0);
            risa_compiler_emit_byte(compiler, 0);

            #undef D_TYPE
        }
    }

    risa_compiler_finalize_compilation(compiler);

    return compiler->parser->error ? RISA_COMPILER_STATUS_ERROR : RISA_COMPILER_STATUS_OK;
}

static void risa_compiler_compile_byte(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t reg = risa_compiler_register_find(compiler, RISA_REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!risa_compiler_register_reserve(compiler))
            return;

        int64_t num;

        if (!risa_lib_charlib_strntoll(compiler->parser->previous.start, compiler->parser->previous.size, 10, &num) || num > UINT8_MAX) {
            risa_parser_error_at_previous(compiler->parser, "Number is invalid for type 'byte'");
            return;
        }

        risa_compiler_emit_constant(compiler, RISA_BYTE_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, compiler->parser->previous };
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    }
}

static void risa_compiler_compile_int(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t reg = risa_compiler_register_find(compiler, RISA_REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if(!risa_compiler_register_reserve(compiler))
            return;

        int64_t num;

        if(!risa_lib_charlib_strntoll(compiler->parser->previous.start, compiler->parser->previous.size, 10, &num)) {
            risa_parser_error_at_previous(compiler->parser, "Number is invalid for type 'int'");
            return;
        }

        risa_compiler_emit_constant(compiler, RISA_INT_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, compiler->parser->previous };
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    }
}

static void risa_compiler_compile_float(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t reg = risa_compiler_register_find(compiler, RISA_REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if(!risa_compiler_register_reserve(compiler))
            return;

        double num;

        if(!risa_lib_charlib_strntod(compiler->parser->previous.start, compiler->parser->previous.size, &num)) {
            risa_parser_error_at_previous(compiler->parser, "Number is invalid for type 'float'");
            return;
        }

        risa_compiler_emit_constant(compiler, RISA_FLOAT_VALUE(num));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, compiler->parser->previous };
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    }
}

static void risa_compiler_compile_string(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t reg = risa_compiler_register_find(compiler, RISA_REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!risa_compiler_register_reserve(compiler))
            return;

        const char *start = compiler->parser->previous.start + 1;
        uint32_t length = compiler->parser->previous.size - 2;

        const char *ptr = start;
        const char *end = start + length;

        uint32_t escapeCount = 0;

        while (ptr < end)
            if (*(ptr++) == '\\')
                ++escapeCount;

        char *str = (char *) RISA_MEM_ALLOC(length + 1 - escapeCount);
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
                            RISA_WARNING(compiler->io, "Invalid escape sequence at index %d", compiler->parser->previous.index + 1 + i);
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

        RisaCompiler *super = compiler->super == NULL ? compiler : compiler->super;

        while (super->super != NULL)
            super = super->super;

        RisaDenseString *interned = risa_map_find(&super->strings, start, length, hash);

        if (interned == NULL) {
            interned = risa_dense_string_from(start, length);
            risa_map_set(&super->strings, interned, RISA_NULL_VALUE);
        }

        RISA_MEM_FREE(str);

        risa_compiler_emit_constant(compiler, RISA_DENSE_VALUE(interned));

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, compiler->parser->previous };
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    }
}

static void risa_compiler_compile_literal(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t reg = risa_compiler_register_find(compiler, RISA_REG_CONSTANT, compiler->parser->previous);

    if(reg == 251) {
        if (!risa_compiler_register_reserve(compiler))
            return;

        switch (compiler->parser->previous.type) {
            case RISA_TOKEN_NULL:
                risa_compiler_emit_byte(compiler, RISA_OP_NULL);
                break;
            case RISA_TOKEN_TRUE:
                risa_compiler_emit_byte(compiler, RISA_OP_TRUE);
                break;
            case RISA_TOKEN_FALSE:
                risa_compiler_emit_byte(compiler, RISA_OP_FALSE);
                break;
            default:
                return;
        }

        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, 0);
        risa_compiler_emit_byte(compiler, 0);

        compiler->last.reg = compiler->regIndex - 1;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, compiler->parser->previous };
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    } else {
        compiler->last.reg = reg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = false;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;
    }
}

static void risa_compiler_compile_identifier(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t get;
    uint8_t set;

    uint8_t index = risa_compiler_local_resolve(compiler, &compiler->parser->previous);

    if(index != 251) {
        get = RISA_OP_MOV;
        set = RISA_OP_MOV;
    } else {
        index = risa_compiler_upvalue_resolve(compiler, &compiler->parser->previous);

        if(index != 251) {
            get = RISA_OP_GUPVAL;
            set = RISA_OP_SUPVAL;
        } else {
            index = risa_compiler_create_identifier_constant(compiler);

            get = RISA_OP_GGLOB;
            set = RISA_OP_SGLOB;
        }
    }

    if(allowAssignment && (compiler->parser->current.type == RISA_TOKEN_EQUAL)) {
        risa_parser_advance(compiler->parser);

        uint32_t clusterSize = compiler->function->cluster.size;
        risa_compiler_compile_expression(compiler);
        RisaCluster cluster = compiler->function->cluster;

        if(set == RISA_OP_MOV) {
            if((clusterSize == cluster.size)                  // Register reference; no new OPs.
            || (clusterSize + 4 == cluster.size               // Only one OP.
              && (cluster.bytecode[clusterSize] == RISA_OP_INC     // OP is INC.
              || (cluster.bytecode[clusterSize] == RISA_OP_DEC)))) // OP is DEC.
                risa_compiler_emit_mov(compiler, index, compiler->last.reg); // MOV the origin.

            else if(compiler->last.isPostIncrement) {
                int32_t incOffset = 4;

                while(cluster.size - incOffset >= clusterSize && cluster.bytecode[cluster.size - incOffset] != RISA_OP_INC && cluster.bytecode[cluster.size - incOffset] != RISA_OP_DEC)
                    incOffset += 4;

                if(cluster.size - incOffset < clusterSize) {
                    risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix when it isn't (report this to the developers)");
                    return;
                }

                if(cluster.size - incOffset - 4 < clusterSize || cluster.bytecode[cluster.size - incOffset - 4] != RISA_OP_MOV) {
                    risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not MOV (report this to the developers)");
                    return;
                }

                if(cluster.bytecode[cluster.size - incOffset + 1] == compiler->localCount - 1) { // The INC target interferes with the local register.
                    uint8_t dest = compiler->localCount - 1;
                    uint8_t tmp = cluster.bytecode[cluster.size - incOffset - 4 + 1]; // Original MOV target.

                    cluster.bytecode[cluster.size - incOffset - 4 + 2] = tmp;  // MOV source.
                    cluster.bytecode[cluster.size - incOffset - 4 + 1] = dest; // MOV target.
                    cluster.bytecode[cluster.size - incOffset + 1] = tmp;      // INC target.

                    incOffset += 8; // Jump over MOV.

                    if(cluster.size - incOffset >= clusterSize) {
                        if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_GET) {
                            risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not GET (report this to the developers)");
                            return;
                        }

                        cluster.bytecode[cluster.size - incOffset + 1] = tmp;
                    }

                    incOffset -= 4 + 8;

                    if(incOffset > 0) {
                        if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_SET) {
                            risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC successor is not SET (report this to the developers)");
                            return;
                        }

                        cluster.bytecode[cluster.size - incOffset + 3] = tmp;
                    }

                    incOffset -= 4;

                    if(incOffset > 0) {
                        if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_SGLOB) {
                            risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but SET successor is not SGLOB (report this to the developers)");
                            return;
                        }

                        cluster.bytecode[cluster.size - incOffset + 2] = tmp;
                    }
                } else cluster.bytecode[cluster.size - incOffset - 4 + 1] = compiler->localCount - 1; // Directly MOV to local.
            } else if(risa_op_has_direct_dest(cluster.bytecode[cluster.size - 4] & RISA_TODLR_INSTRUCTION_MASK) && !compiler->last.isEqualOp) { // Can directly assign to local.
                cluster.bytecode[cluster.size - 4 + 1] = index;  // Do it.
                compiler->last.reg = index;
            } else {
                risa_compiler_emit_mov(compiler, index, cluster.bytecode[cluster.size - 3]);
            }/*compiler->function->cluster.bytecode[compiler->function->cluster.size - 3] = index;*/
        } else {
            if(set == RISA_OP_SGLOB) {
                compiler->last.isConstOptimized = risa_compiler_can_optimize_last_cnst(compiler);
                risa_compiler_optimize_last_cnst(compiler);

                // TODO: check if this works for all cases
                /*if(cluster->bytecode[cluster->size - 4] == RISA_OP_CNST) {
                    compiler->last.reg = cluster->bytecode[cluster->size - 2];
                    compiler->last.isConst = true;
                    cluster->size -= 4;
                }*/

                #define L_TYPE ((compiler->last.isConstOptimized) * RISA_TODLR_TYPE_LEFT_MASK)

                set |= L_TYPE;

                #undef L_TYPE
            }

            risa_compiler_emit_byte(compiler, set);
            risa_compiler_emit_byte(compiler, index);
            risa_compiler_emit_byte(compiler, compiler->last.reg); // compiler->regIndex - 1;
            risa_compiler_emit_byte(compiler, 0);
        }

        compiler->last.isConst = false;
        compiler->last.fromBranched = false;

        //risa_compiler_register_free(compiler);
    } else {
        uint8_t reg = risa_compiler_register_find(compiler, get == RISA_OP_MOV ? RISA_REG_LOCAL :
                                                            (get == RISA_OP_GUPVAL ? RISA_REG_UPVAL : RISA_REG_GLOBAL),
                                                  compiler->parser->previous);

        // Eliminate GGLOB after DGLOB
        if(reg == 251) {
            RisaCluster* cluster = &compiler->function->cluster;

            if(cluster->size > 0 && cluster->bytecode[cluster->size - 4] == RISA_OP_DGLOB)
                if(cluster->bytecode[cluster->size - 4 + 1] == index)
                    reg = cluster->bytecode[cluster->size - 4 + 2];
        }

        if(reg == 251) {
            if(!risa_compiler_register_reserve(compiler))
                return;

            if(get == RISA_OP_MOV)
                risa_compiler_emit_mov(compiler, compiler->regIndex - 1, index);
            else {
                risa_compiler_emit_byte(compiler, get);
                risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
                risa_compiler_emit_byte(compiler, index);
                risa_compiler_emit_byte(compiler, 0);
            }

            compiler->last.reg = compiler->regIndex - 1;
            compiler->regs[compiler->last.reg] = (RisaRegInfo) { get == RISA_OP_MOV ? RISA_REG_LOCAL : (
                                                                     get == RISA_OP_GUPVAL ? RISA_REG_UPVAL :
                                                                     RISA_REG_GLOBAL),
                                                                 compiler->parser->previous };
            compiler->last.isNew = true;
        } else {
            compiler->last.reg = reg;

            if(reg == compiler->regIndex) {
                if (!risa_compiler_register_reserve(compiler))
                    return;
                compiler->last.isNew = true;
            } else compiler->last.isNew = false;
        }

        switch(get) {
            case RISA_OP_MOV:
                compiler->last.lvalMeta.type = LVAL_LOCAL;
                break;
            case RISA_OP_GGLOB:
                compiler->last.lvalMeta.type = LVAL_GLOBAL;
                compiler->last.lvalMeta.global = index;
                compiler->last.lvalMeta.globalReg = reg == 251 ? compiler->regIndex - 1 : reg;
                break;
            case RISA_OP_GUPVAL:
                compiler->last.lvalMeta.type = LVAL_UPVAL;
                compiler->last.lvalMeta.upval = index;
                break;
            default:
                break;
        }

        compiler->last.isConstOptimized = false;
    }

    compiler->last.isConst = false;
    compiler->last.isLvalue = true;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;
}

static void risa_compiler_compile_array(RisaCompiler* compiler, bool allowAssignment) {
    if(!risa_compiler_register_reserve(compiler))
        return;

    uint8_t reg = compiler->regIndex - 1;

    risa_compiler_emit_byte(compiler, RISA_OP_ARR);
    risa_compiler_emit_byte(compiler, reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->parser->current.type != RISA_TOKEN_RIGHT_BRACKET) {
        while (1) {
            risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA + 1);

            if (compiler->last.isNew)
                risa_compiler_register_free(compiler);

            bool isOptimized = risa_compiler_can_optimize_last_cnst(compiler);

            risa_compiler_optimize_last_cnst(compiler);

            #define L_TYPE (isOptimized * RISA_TODLR_TYPE_LEFT_MASK)

            risa_compiler_emit_byte(compiler, RISA_OP_PARR | L_TYPE);
            risa_compiler_emit_byte(compiler, reg);
            risa_compiler_emit_byte(compiler, compiler->last.reg);
            risa_compiler_emit_byte(compiler, 0);

            #undef L_TYPE

            if (compiler->parser->current.type == RISA_TOKEN_RIGHT_BRACKET || compiler->parser->current.type == RISA_TOKEN_EOF)
                break;

            risa_parser_advance(compiler->parser);
        }
    }

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_BRACKET, "Expected ']' after array contents");

    compiler->last.reg = reg;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_TEMP };
}

static void risa_compiler_compile_object(RisaCompiler* compiler, bool allowAssignment) {
    if(!risa_compiler_register_reserve(compiler))
        return;

    uint8_t reg = compiler->regIndex - 1;

    risa_compiler_emit_byte(compiler, RISA_OP_OBJ);
    risa_compiler_emit_byte(compiler, reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->parser->current.type != RISA_TOKEN_RIGHT_BRACE) {
        while (1) {
            uint8_t dest;
            bool isConst;

            switch (compiler->parser->current.type) {
                case RISA_TOKEN_IDENTIFIER: {
                    RisaToken prop = compiler->parser->current;
                    risa_parser_advance(compiler->parser);

                    if(prop.size == 6 && memcmp(prop.start, "length", 6) == 0) {
                        risa_parser_error_at_previous(compiler->parser, "The property 'length' is reserved");
                        return;
                    }

                    uint16_t propIndex = risa_compiler_create_identifier_constant(compiler);

                    if(propIndex < UINT8_MAX) {
                        isConst = true;

                        dest = propIndex;
                    } else {
                        isConst = false;

                        if (!risa_compiler_register_reserve(compiler))
                            return;

                        risa_compiler_emit_byte(compiler, RISA_OP_CNSTW);
                        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
                        risa_compiler_emit_word(compiler, propIndex);

                        dest = compiler->regIndex - 1;
                    }

                    break;
                }
                case RISA_TOKEN_STRING: {
                    RisaToken prop = compiler->parser->current;
                    risa_parser_advance(compiler->parser);

                    if(prop.size == 8 && memcmp(prop.start, "\"length\"", 8) == 0) {
                        risa_parser_error_at_previous(compiler->parser, "The property 'length' is reserved");
                        return;
                    }

                    size_t clusterSize = compiler->function->cluster.size;

                    risa_compiler_compile_string(compiler, allowAssignment);

                    if(compiler->function->cluster.size != clusterSize) {
                        dest = compiler->function->cluster.bytecode[compiler->function->cluster.size - 4 + 2];
                        compiler->function->cluster.size = clusterSize;
                    } else dest = compiler->last.reg;

                    isConst = compiler->last.isConst;

                    break;
                }
                default:
                    risa_parser_error_at_current(compiler->parser, "Expected identifier or string");
                    return;
            }

            risa_parser_consume(compiler->parser, RISA_TOKEN_COLON, "Expected ':' after object key");

            risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA + 1);

            if(compiler->last.isNew)
                risa_compiler_register_free(compiler);

            bool isOptimized = risa_compiler_can_optimize_last_cnst(compiler);

            risa_compiler_optimize_last_cnst(compiler);

            #define LR_TYPES ((isConst * RISA_TODLR_TYPE_LEFT_MASK) | (isOptimized * RISA_TODLR_TYPE_RIGHT_MASK))

            risa_compiler_emit_byte(compiler, RISA_OP_SET | LR_TYPES);//HERE2
            risa_compiler_emit_byte(compiler, reg);
            risa_compiler_emit_byte(compiler, dest);
            risa_compiler_emit_byte(compiler, compiler->last.reg);

            #undef LR_TYPES

            if(compiler->parser->current.type == RISA_TOKEN_RIGHT_BRACE || compiler->parser->current.type == RISA_TOKEN_EOF)
                break;

            risa_parser_consume(compiler->parser, RISA_TOKEN_COMMA, "Expected ',' after object entry");
        }
    }

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_BRACE, "Expected '}' after object properties");

    compiler->last.reg = reg;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_TEMP };
}

static void risa_compiler_compile_declaration(RisaCompiler* compiler) {
    size_t regIndex = compiler->regIndex;
    size_t localCount = compiler->localCount;

    if(compiler->parser->current.type == RISA_TOKEN_VAR) {
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_variable_declaration(compiler);
    } else if(compiler->parser->current.type == RISA_TOKEN_FUNCTION) {
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_function_declaration(compiler);
    } else risa_compiler_compile_statement(compiler);

    if(compiler->parser->panic)
        risa_parser_sync(compiler->parser);

    if(compiler->regIndex - regIndex != compiler->localCount - localCount)
        compiler->regIndex = regIndex + (compiler->localCount - localCount);
}

static void risa_compiler_compile_variable_declaration(RisaCompiler* compiler) {
    uint16_t index = risa_compiler_declare_variable(compiler);
    uint32_t clusterSize = compiler->function->cluster.size;
    RisaToken lastRegToken = compiler->parser->previous;

    if(compiler->parser->current.type == RISA_TOKEN_EQUAL) {
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_expression(compiler);
    } else {
        if(!risa_compiler_register_reserve(compiler))
            return;

        risa_compiler_emit_byte(compiler, RISA_OP_NULL);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, 0);
        risa_compiler_emit_byte(compiler, 0);

        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.fromBranched = false;
    }

    risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after variable declaration");

    if(compiler->scopeDepth > 0) {
        RisaCluster cluster = compiler->function->cluster;

        if((clusterSize == cluster.size)                  // Register reference; no new OPs.
        || (clusterSize + 4 == cluster.size               // Only one OP.
          && (cluster.bytecode[clusterSize] == RISA_OP_INC     // OP is INC.
          || (cluster.bytecode[clusterSize] == RISA_OP_DEC)))) // OP is DEC.
            risa_compiler_emit_mov(compiler, compiler->localCount - 1, compiler->last.reg); // MOV the origin.

        else if(compiler->last.isPostIncrement) {
            int32_t incOffset = 4;

            while(cluster.size - incOffset >= clusterSize && cluster.bytecode[cluster.size - incOffset] != RISA_OP_INC && cluster.bytecode[cluster.size - incOffset] != RISA_OP_DEC)
                incOffset += 4;

            if(cluster.size - incOffset < clusterSize) {
                risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix when it isn't (report this to the developers)");
                return;
            }

            if(cluster.size - incOffset - 4 < clusterSize || cluster.bytecode[cluster.size - incOffset - 4] != RISA_OP_MOV) {
                risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not MOV (report this to the developers)");
                return;
            }

            if(cluster.bytecode[cluster.size - incOffset + 1] == compiler->localCount - 1) { // The INC target interferes with the local register.
                uint8_t dest = compiler->localCount - 1;
                uint8_t tmp = cluster.bytecode[cluster.size - incOffset - 4 + 1]; // Original MOV target.

                cluster.bytecode[cluster.size - incOffset - 4 + 2] = tmp;  // MOV source.
                cluster.bytecode[cluster.size - incOffset - 4 + 1] = dest; // MOV target.
                cluster.bytecode[cluster.size - incOffset + 1] = tmp;      // INC target.

                incOffset += 8; // Jump over MOV.

                if(cluster.size - incOffset >= clusterSize) {
                    if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_GET) {
                        risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC predecessor is not GET (report this to the developers)");
                        return;
                    }

                    cluster.bytecode[cluster.size - incOffset + 1] = tmp;
                }

                incOffset -= 4 + 8;

                if(incOffset > 0) {
                    if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_SET) {
                        risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but INC successor is not SET (report this to the developers)");
                        return;
                    }

                    cluster.bytecode[cluster.size - incOffset + 3] = tmp;
                }

                incOffset -= 4;

                if(incOffset > 0) {
                    if((cluster.bytecode[cluster.size - incOffset] & RISA_TODLR_INSTRUCTION_MASK) != RISA_OP_SGLOB) {
                        risa_parser_error_at_current(compiler->parser, "PANIC: Last was marked as postfix, but SET successor is not SGLOB (report this to the developers)");
                        return;
                    }

                    cluster.bytecode[cluster.size - incOffset + 2] = tmp;
                }
            } else cluster.bytecode[cluster.size - incOffset - 4 + 1] = compiler->localCount - 1; // Directly MOV to local.
        } else if(risa_op_has_direct_dest(cluster.bytecode[cluster.size - 4] & RISA_TODLR_INSTRUCTION_MASK) && !compiler->last.isEqualOp) { // Can directly assign to local.
            cluster.bytecode[cluster.size - 4 + 1] = compiler->localCount - 1;  // Do it.
            compiler->last.reg = index;
        } else risa_compiler_emit_mov(compiler, compiler->localCount - 1, compiler->last.reg); // MOV the result.

        if(compiler->last.isNew)
            risa_compiler_register_free(compiler);

        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;

        compiler->last.reg = compiler->localCount - 1;
        compiler->last.isConstOptimized = false;
        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_LOCAL, lastRegToken };
        compiler->last.isNew = true;
        compiler->last.isLvalue = false;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;

        if(compiler->regIndex == 249) {
            risa_parser_error_at_current(compiler->parser, "Register limit exceeded (250)");
            return;
        }

        ++compiler->regIndex;
        return;
    }

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    compiler->last.isConstOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    // TODO: check if this ^^^^^ is a perfect replacement.
    /*if(cluster->bytecode[cluster->size - 4] == RISA_OP_CNST) {
        compiler->last.reg = cluster->bytecode[cluster->size - 2];
        compiler->last.isConst = true;
        cluster->size -= 4;
    }*/

    #define L_TYPE (compiler->last.isConstOptimized * RISA_TODLR_TYPE_LEFT_MASK)

    risa_compiler_emit_byte(compiler, RISA_OP_DGLOB | L_TYPE);
    risa_compiler_emit_byte(compiler, index);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);

    #undef L_TYPE

    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;
}

static void risa_compiler_compile_function_declaration(RisaCompiler* compiler) {
    uint16_t index = risa_compiler_declare_variable(compiler);

    if(!risa_compiler_register_reserve(compiler))
        return;

    if(compiler->scopeDepth > 0)
        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;

    risa_compiler_compile_function(compiler);

    if(compiler->scopeDepth > 0)
        return;

    compiler->last.isConstOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    // TODO: Check if this ^^^^^^ is a good replacement.
    /*if(cluster->bytecode[cluster->size - 4] == RISA_OP_CNST) {
        compiler->last.reg = cluster->bytecode[cluster->size - 2];
        compiler->last.isConst = true;
        cluster->size -= 4;
    }*/

    if(!compiler->last.isConstOptimized)
        compiler->last.reg = compiler->regIndex - 1;

    risa_compiler_register_free(compiler);

    #define L_TYPE (compiler->last.isConstOptimized * RISA_TODLR_TYPE_LEFT_MASK)

    risa_compiler_emit_byte(compiler, RISA_OP_DGLOB | L_TYPE);
    risa_compiler_emit_byte(compiler, (uint8_t) index);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);

    #undef L_TYPE

    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;
}

static void risa_compiler_compile_function(RisaCompiler* compiler) {
    RisaCompiler subcompiler;
    risa_compiler_init(&subcompiler);

    const char* start = compiler->parser->previous.start;
    uint32_t length = compiler->parser->previous.size;
    uint32_t hash = risa_map_hash(start, length);

    RisaCompiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(&super->strings, interned, RISA_NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.super = compiler;
    subcompiler.parser = compiler->parser;

    risa_compiler_scope_begin(&subcompiler);
    risa_parser_consume(subcompiler.parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if(subcompiler.parser->current.type != RISA_TOKEN_RIGHT_PAREN) {
        do {
            ++subcompiler.function->arity;

            if (subcompiler.function->arity > 250) {
                risa_parser_error_at_current(subcompiler.parser, "Parameter limit exceeded (250)");
                return;
            }

            risa_compiler_declare_variable(&subcompiler);

            subcompiler.locals[subcompiler.localCount - 1].depth = subcompiler.scopeDepth;

            ++subcompiler.regIndex;

            if(subcompiler.parser->current.type != RISA_TOKEN_COMMA)
                break;

            risa_parser_advance(subcompiler.parser);
        } while(true);
    }

    risa_parser_consume(subcompiler.parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after parameters");

    if(subcompiler.parser->current.type == RISA_TOKEN_EQUAL_GREATER) {
        risa_parser_advance(subcompiler.parser);
        risa_compiler_compile_return_statement(&subcompiler);
    } else {
        risa_parser_consume(subcompiler.parser, RISA_TOKEN_LEFT_BRACE, "Expected '{' before function body");
        risa_compiler_compile_block(&subcompiler);
        risa_compiler_emit_return(&subcompiler);
    }

    if(compiler->scopeDepth == 0)
        if(!risa_compiler_register_reserve(compiler))
            return;

    if(subcompiler.upvalueCount == 0) {
        risa_compiler_emit_constant(compiler, RISA_DENSE_VALUE(subcompiler.function));
    } else {
        risa_compiler_emit_constant(compiler, RISA_DENSE_VALUE(subcompiler.function));

        risa_compiler_emit_byte(compiler, RISA_OP_CLSR);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, subcompiler.upvalueCount);

        for(uint8_t i = 0; i < subcompiler.upvalueCount; ++i) {
            risa_compiler_emit_byte(compiler, RISA_OP_UPVAL);
            risa_compiler_emit_byte(compiler, subcompiler.upvalues[i].index);
            risa_compiler_emit_byte(compiler, subcompiler.upvalues[i].local);
            risa_compiler_emit_byte(compiler, 0);
        }
    }

    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    risa_compiler_delete(&subcompiler);
}

static void risa_compiler_compile_statement(RisaCompiler* compiler) {
    switch(compiler->parser->current.type) {
        case RISA_TOKEN_IF:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_if_statement(compiler);
            break;
        case RISA_TOKEN_WHILE:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_while_statement(compiler);
            break;
        case RISA_TOKEN_FOR:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_for_statement(compiler);
            break;
        case RISA_TOKEN_RETURN:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_return_statement(compiler);
            break;
        case RISA_TOKEN_CONTINUE:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_continue_statement(compiler);
            break;
        case RISA_TOKEN_BREAK:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_break_statement(compiler);
            break;
        case RISA_TOKEN_LEFT_BRACE:
            risa_parser_advance(compiler->parser);

            risa_compiler_scope_begin(compiler);
            risa_compiler_compile_block(compiler);
            risa_compiler_scope_end(compiler);
            break;
        case RISA_TOKEN_DOLLAR:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_inline_asm_statement(compiler);
            break;
        case RISA_TOKEN_PERCENT:
            risa_parser_advance(compiler->parser);
            risa_compiler_compile_disasm_statement(compiler);
            break;
        case RISA_TOKEN_SEMICOLON:
            risa_parser_advance(compiler->parser);
            break;
        default:
            risa_compiler_compile_expression_statement(compiler);
            break;
    }
}

static void risa_compiler_compile_if_statement(RisaCompiler* compiler) {
    risa_parser_consume(compiler->parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    risa_compiler_compile_expression(compiler);
    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    risa_compiler_emit_byte(compiler, RISA_OP_TEST);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    uint32_t ifEnd = risa_compiler_emit_blank(compiler);

    risa_compiler_compile_statement(compiler);

    if(compiler->parser->current.type == RISA_TOKEN_ELSE) {
        uint32_t elseEnd = risa_compiler_emit_blank(compiler);

        risa_compiler_emit_jump(compiler, ifEnd);
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_statement(compiler);
        risa_compiler_emit_jump(compiler, elseEnd);
    } else risa_compiler_emit_jump(compiler, ifEnd);
}

static void risa_compiler_compile_while_statement(RisaCompiler* compiler) {
    uint32_t start = compiler->function->cluster.size;

    risa_parser_consume(compiler->parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    risa_compiler_compile_expression(compiler);
    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    risa_compiler_emit_byte(compiler, RISA_OP_TEST);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    uint32_t end = risa_compiler_emit_blank(compiler);

    if(compiler->loopCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "Loop limit exceeded (250)");
        return;
    }

    ++compiler->loopCount;

    for(uint8_t i = 0; i < compiler->leapCount; ++i)
        ++compiler->leaps[i].depth;

    risa_compiler_compile_statement(compiler);

    risa_compiler_emit_backwards_jump(compiler, start);
    risa_compiler_emit_jump(compiler, end);

    for(uint8_t i = 0; i < compiler->leapCount; ++i) {
        RisaLeapInfo* leap = &compiler->leaps[i];
        --leap->depth;

        if(leap->depth == 0) {
            if(leap->isBreak)
                risa_compiler_emit_jump(compiler, leap->index);
            else risa_compiler_emit_backwards_jump_from(compiler, leap->index, start);
        }
    }
}

static void risa_compiler_compile_for_statement(RisaCompiler* compiler) {
    risa_compiler_scope_begin(compiler);

    risa_parser_consume(compiler->parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after 'for'");

    if(compiler->parser->current.type == RISA_TOKEN_SEMICOLON) {
        risa_parser_advance(compiler->parser);
    } else if(compiler->parser->current.type == RISA_TOKEN_VAR) {
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_variable_declaration(compiler);
    } else {
        risa_parser_advance(compiler->parser);
        risa_compiler_compile_expression_statement(compiler);
    }

    uint32_t start = compiler->function->cluster.size;
    uint32_t exitIndex = 0;
    bool infinite = true;

    if(compiler->parser->current.type != RISA_TOKEN_SEMICOLON) {
        risa_compiler_compile_expression(compiler);
        risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after loop condition");

        risa_compiler_emit_byte(compiler, RISA_OP_TEST);
        risa_compiler_emit_byte(compiler, compiler->last.reg);
        risa_compiler_emit_byte(compiler, 0);
        risa_compiler_emit_byte(compiler, 0);

        if(compiler->last.isNew)
            risa_compiler_register_free(compiler);

        exitIndex = risa_compiler_emit_blank(compiler);
        infinite = false;
    }

    if(compiler->parser->current.type != RISA_TOKEN_RIGHT_PAREN) {
        uint32_t bodyJump = risa_compiler_emit_blank(compiler);
        uint32_t post = compiler->function->cluster.size;
        uint8_t regIndex = compiler->regIndex;

        risa_compiler_compile_expression(compiler);

        if(regIndex != compiler->regIndex)
            risa_compiler_register_free(compiler);

        risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after clauses");

        risa_compiler_emit_backwards_jump(compiler, start);
        start = post;
        risa_compiler_emit_jump(compiler, bodyJump);
    }

    if(compiler->loopCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "Loop limit exceeded (250)");
        return;
    }

    ++compiler->loopCount;

    for(uint8_t i = 0; i < compiler->leapCount; ++i)
        ++compiler->leaps[i].depth;

    risa_compiler_compile_statement(compiler);
    risa_compiler_emit_backwards_jump(compiler, start);

    if(!infinite) {
        risa_compiler_emit_jump(compiler, exitIndex);
    }

    for(uint8_t i = 0; i < compiler->leapCount; ++i) {
        RisaLeapInfo* leap = &compiler->leaps[i];
        --leap->depth;

        if(leap->depth == 0) {
            if(leap->isBreak)
                risa_compiler_emit_jump(compiler, leap->index);
            else risa_compiler_emit_backwards_jump_from(compiler, leap->index, start);
        }
    }

    risa_compiler_scope_end(compiler);
}

static void risa_compiler_compile_return_statement(RisaCompiler* compiler) {
    if(compiler->function->name == NULL) {
        risa_parser_error_at_previous(compiler->parser, "Cannot return from top-level scope");
    }

    if(compiler->parser->current.type == RISA_TOKEN_SEMICOLON) {
        risa_parser_advance(compiler->parser);
        risa_compiler_emit_return(compiler);
    } else {
        risa_compiler_compile_expression(compiler);
        risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after return expression");

        risa_compiler_emit_byte(compiler, RISA_OP_RET);
        risa_compiler_emit_byte(compiler, compiler->last.reg);
        risa_compiler_emit_byte(compiler, 0);
        risa_compiler_emit_byte(compiler, 0);

        if(compiler->last.isNew)
            risa_compiler_register_free(compiler);
    }
}

static void risa_compiler_compile_return_expression(RisaCompiler* compiler) {
    if(compiler->function->name == NULL) {
        risa_parser_error_at_previous(compiler->parser, "Cannot return from top-level scope");
    }

    if(compiler->parser->current.type == RISA_TOKEN_SEMICOLON) {
        risa_compiler_emit_return(compiler);
    } else {
        risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA + 1);

        risa_compiler_emit_byte(compiler, RISA_OP_RET);
        risa_compiler_emit_byte(compiler, compiler->last.reg);
        risa_compiler_emit_byte(compiler, 0);
        risa_compiler_emit_byte(compiler, 0);

        if(compiler->last.isNew)
            risa_compiler_register_free(compiler);
    }
}

static void risa_compiler_compile_continue_statement(RisaCompiler* compiler) {
    if(compiler->loopCount == 0) {
        risa_parser_error_at_previous(compiler->parser, "Cannot continue outside of loops");
        return;
    }
    if(compiler->leapCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "RisaLeapInfo limit exceeded (250)");
        return;
    }

    RisaLeapInfo leap;
    leap.isBreak = false;
    leap.index = compiler->function->cluster.size;

    if(compiler->parser->current.type == RISA_TOKEN_SEMICOLON) {
        risa_parser_advance(compiler->parser);
        leap.depth = 1;
    } else if(compiler->parser->current.type == RISA_TOKEN_INT) {
        risa_parser_advance(compiler->parser);

        int64_t num;

        if(!risa_lib_charlib_strntoll(compiler->parser->previous.start, compiler->parser->previous.size, 10, &num)) {
            risa_parser_error_at_previous(compiler->parser, "Number is invalid for type 'int'");
            return;
        }
        if(num < 0) {
            risa_parser_error_at_previous(compiler->parser, "Continue depth cannot be negative");
            return;
        }
        if(num > compiler->loopCount) {
            risa_parser_error_at_previous(compiler->parser, "Cannot continue from that many loops; consider using 'continue 0;'");
            return;
        }

        if(num == 0)
            leap.depth = compiler->loopCount;
        else leap.depth = (uint8_t) num;

        risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after continue statement");
    } else {
        risa_parser_error_at_previous(compiler->parser, "Expected ';' or number after 'continue'");
        return;
    }

    compiler->leaps[compiler->leapCount++] = leap;
    risa_compiler_emit_blank(compiler);
}

static void risa_compiler_compile_break_statement(RisaCompiler* compiler) {
    if(compiler->loopCount == 0) {
        risa_parser_error_at_previous(compiler->parser, "Cannot break outside of loops");
        return;
    }
    if(compiler->leapCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "RisaLeapInfo limit exceeded (250)");
        return;
    }

    RisaLeapInfo leap;
    leap.isBreak = true;
    leap.index = compiler->function->cluster.size;

    if(compiler->parser->current.type == RISA_TOKEN_SEMICOLON) {
        risa_parser_advance(compiler->parser);
        leap.depth = 1;
    } else if(compiler->parser->current.type == RISA_TOKEN_INT) {
        risa_parser_advance(compiler->parser);

        int64_t num;

        if(!risa_lib_charlib_strntoll(compiler->parser->previous.start, compiler->parser->previous.size, 10, &num)) {
            risa_parser_error_at_previous(compiler->parser, "Number is too large for type 'int'");
            return;
        }
        if(num < 0) {
            risa_parser_error_at_previous(compiler->parser, "Break depth cannot be negative");
            return;
        }
        if(num > compiler->loopCount) {
            risa_parser_error_at_previous(compiler->parser, "Cannot break from that many loops; consider using 'break 0;'");
            return;
        }

        if(num == 0)
            leap.depth = compiler->loopCount;
        else leap.depth = (uint8_t) num;

        risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after break statement");
    } else {
        risa_parser_error_at_previous(compiler->parser, "Expected ';' or number after 'break'");
        return;
    }

    compiler->leaps[compiler->leapCount++] = leap;
    risa_compiler_emit_blank(compiler);
}

static void risa_compiler_compile_block(RisaCompiler* compiler) {
    while(compiler->parser->current.type != RISA_TOKEN_EOF && compiler->parser->current.type != RISA_TOKEN_RIGHT_BRACE) {
        risa_compiler_compile_declaration(compiler);
    }

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void risa_compiler_compile_inline_asm_statement(RisaCompiler* compiler) {
    bool isBlock = false;

    if(compiler->parser->current.type == RISA_TOKEN_LEFT_BRACE) {
        isBlock = true;
        risa_parser_advance(compiler->parser);
    }

    uint32_t indexBackup = compiler->parser->current.index;

    RisaAssembler iasm;

    risa_assembler_init(&iasm);

    RisaCompiler* super = compiler->super != NULL ? compiler->super : compiler;

    while(super->super != NULL)
        super = super->super;

    iasm.cluster = compiler->function->cluster;
    iasm.strings = &super->strings;

    risa_assembler_assemble(&iasm, compiler->parser->lexer.start, isBlock ? "}" : "\r\n;");

    compiler->function->cluster = iasm.cluster;
    compiler->function->cluster.constants = iasm.cluster.constants;

    compiler->parser->lexer.start = iasm.parser->lexer.start;
    compiler->parser->lexer.current = iasm.parser->lexer.current;
    compiler->parser->lexer.index = indexBackup + iasm.parser->lexer.index;

    if(iasm.parser->error)
        compiler->parser->error = true;

    risa_parser_advance(compiler->parser);

    risa_assembler_delete(&iasm);

    if(isBlock)
        risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_BRACE, "Expected '}' after inline asm block statement");
    else risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after inline asm statement");
}

static void risa_compiler_compile_disasm_statement(RisaCompiler* compiler) {
    bool isSelf = false;

    risa_parser_consume(compiler->parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after '%'");

    if(compiler->parser->current.type == RISA_TOKEN_RIGHT_PAREN)
        isSelf = true;
    else risa_compiler_compile_expression(compiler);

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after argument");
    risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after ')'");

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    risa_compiler_emit_byte(compiler, RISA_OP_DIS);
    risa_compiler_emit_byte(compiler, isSelf ? 251 : compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);
}

static void risa_compiler_compile_expression_statement(RisaCompiler* compiler) {
    risa_compiler_compile_expression(compiler);
    risa_parser_consume(compiler->parser, RISA_TOKEN_SEMICOLON, "Expected ';' after expression");
}

static void risa_compiler_compile_expression(RisaCompiler* compiler) {
    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA);
}

static void risa_compiler_compile_expression_precedence(RisaCompiler* compiler, RisaOperatorPrecedence precedence) {
    risa_parser_advance(compiler->parser);

    RisaOperatorRuleHandler prefix = OPERATOR_RULES[compiler->parser->previous.type].prefix;

    if(prefix == NULL) {
        risa_parser_error_at_previous(compiler->parser, "Expected expression");
        return;
    }

    bool allowAssignment = precedence <= RISA_PREC_ASSIGNMENT;
    prefix(compiler, allowAssignment);

    while(precedence <= OPERATOR_RULES[compiler->parser->current.type].precedence) {
        risa_parser_advance(compiler->parser);

        RisaOperatorRuleHandler infix = OPERATOR_RULES[compiler->parser->previous.type].inpostfix;
        infix(compiler, allowAssignment);
    }

    if(allowAssignment && (compiler->parser->current.type == RISA_TOKEN_EQUAL)) {
        risa_parser_error_at_previous(compiler->parser, "Invalid assignment target");
        return;
    }
}

static void risa_compiler_compile_call(RisaCompiler* compiler, bool allowAssignment) {
    if(!compiler->last.isNew) {
        if(!risa_compiler_register_reserve(compiler))
            return;
        risa_compiler_emit_mov(compiler, compiler->regIndex - 1, compiler->last.reg);
        compiler->last.reg = compiler->regIndex - 1;
    }

    uint8_t functionReg = compiler->last.reg;

    if(compiler->regIndex <= functionReg)
        if(!risa_compiler_register_reserve(compiler))
            return;

    compiler->last.canOverwrite = true;

    uint8_t argc = risa_compiler_compile_arguments(compiler);

    compiler->last.canOverwrite = false;

    risa_compiler_emit_byte(compiler, RISA_OP_CALL);
    risa_compiler_emit_byte(compiler, functionReg);
    risa_compiler_emit_byte(compiler, argc);
    risa_compiler_emit_byte(compiler, 0);

    while(argc > 0) {
        risa_compiler_register_free(compiler);
        --argc;
    }

    compiler->last.reg = functionReg;
    compiler->regs[functionReg] = (RisaRegInfo) {RISA_REG_TEMP };
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;
}

static void risa_compiler_compile_clone(RisaCompiler* compiler, bool allowAssignment) {
    risa_parser_consume(compiler->parser, RISA_TOKEN_LEFT_PAREN, "Expected '(' after 'clone' keyword");
    risa_compiler_compile_expression(compiler);
    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after clone argument");

    uint8_t destReg;

    if(compiler->last.isNew) {
        destReg = compiler->last.reg;

        risa_compiler_register_free(compiler);
    } else {
        if(!risa_compiler_register_reserve(compiler))
            return;
        destReg = compiler->regIndex - 1;
    }

    risa_compiler_emit_byte(compiler, RISA_OP_CLONE);
    risa_compiler_emit_byte(compiler, destReg);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);

    compiler->last.reg = destReg;
    compiler->regs[destReg] = (RisaRegInfo) {RISA_REG_TEMP };
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;
}

static void risa_compiler_compile_dot(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t leftReg = compiler->last.reg;
    bool leftNew = compiler->last.isNew;

    if(compiler->parser->current.type != RISA_TOKEN_IDENTIFIER) {
        risa_parser_error_at_current(compiler->parser, "Expected identifier");
        return;
    }

    RisaToken prop = compiler->parser->current;
    risa_parser_advance(compiler->parser);

    uint16_t propIndex = risa_compiler_create_identifier_constant(compiler);
    bool identifierConst;

    if(propIndex < UINT8_MAX) {
        identifierConst = true;

        compiler->last.reg = propIndex;
        compiler->last.isNew = false;
    } else {
        identifierConst = false;

        if(!risa_compiler_register_reserve(compiler))
            return;

        risa_compiler_emit_byte(compiler, RISA_OP_CNSTW);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_word(compiler, propIndex);

        compiler->last.reg = compiler->regIndex -1;
        compiler->last.isNew = true;
    }

    if(allowAssignment && (compiler->parser->current.type == RISA_TOKEN_EQUAL)) {
        if(prop.size == 6 && memcmp(prop.start, "length", sizeof("length") - 1) == 0) {
            risa_parser_error_at_previous(compiler->parser, "Cannot assign to length");
            return;
        }

        uint8_t rightReg = compiler->last.reg;
        bool rightNew = compiler->last.isNew;

        risa_parser_advance(compiler->parser);
        risa_compiler_compile_expression(compiler);

        compiler->last.isConstOptimized = risa_compiler_can_optimize_last_cnst(compiler);

        // TODO: Test this for all cases.
        risa_compiler_optimize_last_cnst(compiler);

        #define LR_TYPES (identifierConst * RISA_TODLR_TYPE_LEFT_MASK) | (compiler->last.isConstOptimized * RISA_TODLR_TYPE_RIGHT_MASK)

        risa_compiler_emit_byte(compiler, RISA_OP_SET | LR_TYPES);
        risa_compiler_emit_byte(compiler, leftReg);
        risa_compiler_emit_byte(compiler, rightReg);
        risa_compiler_emit_byte(compiler, compiler->last.reg);

        #undef LR_TYPES

        if(leftNew)
            risa_compiler_register_free(compiler);
        if(rightNew)
            risa_compiler_register_free(compiler);
        if(!compiler->last.canOverwrite || compiler->last.lvalMeta.type == LVAL_GLOBAL)
            risa_compiler_register_free(compiler);

        compiler->last.isLvalue = false;
        compiler->last.isConst = false; // CNST was already handled here.
        compiler->last.fromBranched = false;
    } else {
        uint8_t destReg;

        if(compiler->last.isNew) {
            destReg = compiler->last.reg;

            if(leftNew)
                risa_compiler_register_free(compiler);
        } else if(compiler->last.canOverwrite && leftNew) {
            destReg = leftReg;
        } else {
            risa_compiler_register_reserve(compiler);
            destReg = compiler->regIndex - 1;
        }

        if(prop.size == 6 && memcmp(prop.start, "length", sizeof("length") - 1) == 0) {
            risa_compiler_emit_byte(compiler, RISA_OP_LEN);
            risa_compiler_emit_byte(compiler, destReg);
            risa_compiler_emit_byte(compiler, leftReg);
            risa_compiler_emit_byte(compiler, 0);

            compiler->last.isLvalue = false;
        } else {
            #define R_TYPE (identifierConst * RISA_TODLR_TYPE_RIGHT_MASK)

            risa_compiler_emit_byte(compiler, RISA_OP_GET | R_TYPE);
            risa_compiler_emit_byte(compiler, destReg);
            risa_compiler_emit_byte(compiler, leftReg);
            risa_compiler_emit_byte(compiler, compiler->last.reg);

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
        compiler->last.lvalMeta.propIndex.isConst = identifierConst;

        if(identifierConst) {
            compiler->last.lvalMeta.propIndex.as.cnst = compiler->last.reg;
        } else {
            compiler->last.lvalMeta.propIndex.as.reg = compiler->last.reg;
        }

        compiler->last.reg = destReg;
        compiler->last.isConstOptimized = false;
        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = true;
        compiler->last.isPostIncrement = false;
        compiler->last.isEqualOp = false;
        compiler->last.fromBranched = false;

        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_TEMP };
    }
}

static uint8_t risa_compiler_compile_arguments(RisaCompiler* compiler) {
    uint8_t argc = 0;

    if(compiler->parser->current.type != RISA_TOKEN_RIGHT_PAREN) {
        do {
            uint32_t clusterSize = compiler->function->cluster.size;
            uint8_t regIndex = compiler->regIndex;

            risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA + 1);

            if(regIndex != compiler->regIndex)
                compiler->regIndex = regIndex;

            if(!risa_compiler_register_reserve(compiler))
                return 255;

            if(clusterSize == compiler->function->cluster.size)
                risa_compiler_emit_mov(compiler, compiler->regIndex - 1, compiler->last.reg);
            else if(risa_op_has_direct_dest(compiler->function->cluster.bytecode[compiler->function->cluster.size - 4] & RISA_TODLR_INSTRUCTION_MASK) && !compiler->last.isEqualOp)
                compiler->function->cluster.bytecode[compiler->function->cluster.size - 3] = compiler->regIndex - 1;
            else risa_compiler_emit_mov(compiler, compiler->regIndex - 1, compiler->last.reg);

            if(argc == 255) {
                risa_parser_error_at_previous(compiler->parser, "Argument limit exceeded (255)");
                return 255;
            }

            ++argc;

            if(compiler->parser->current.type != RISA_TOKEN_COMMA)
                break;

            risa_parser_advance(compiler->parser);
        } while(true);
    }

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
    return argc;
}

static void risa_compiler_compile_grouping_or_lambda(RisaCompiler* compiler, bool allowAssignment) {
    uint32_t backupSize = compiler->function->cluster.size;
    RisaParser backupParser = *compiler->parser;

    if(compiler->parser->current.type != RISA_TOKEN_RIGHT_PAREN) {
        risa_compiler_compile_expression(compiler);
        risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after expression");
    } else {
        if(risa_parser_advance(compiler->parser), compiler->parser->current.type != RISA_TOKEN_EQUAL_GREATER) {
            risa_parser_error_at_previous(compiler->parser, "Unexpected empty parentheses group");
            return;
        }

        compiler->function->cluster.size = backupSize;
        memcpy(compiler->parser, &backupParser, sizeof(RisaParser));

        risa_compiler_compile_lambda(compiler);
        return;
    }

    // Lambda.
    if(compiler->parser->current.type == RISA_TOKEN_EQUAL_GREATER) {
        if(compiler->last.isNew)
            risa_compiler_register_free(compiler);

        compiler->function->cluster.size = backupSize;
        memcpy(compiler->parser, &backupParser, sizeof(RisaParser));

        risa_compiler_compile_lambda(compiler);
    }
}

static void risa_compiler_compile_lambda(RisaCompiler* compiler) {
    RisaCompiler subcompiler;
    risa_compiler_init(&subcompiler);

    const char* start = "lambda";
    uint32_t length = 6;
    uint32_t hash = risa_map_hash(start, length);

    RisaCompiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(&super->strings, interned, RISA_NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.super = compiler;
    subcompiler.parser = compiler->parser;

    risa_compiler_scope_begin(&subcompiler);

    if(subcompiler.parser->current.type != RISA_TOKEN_RIGHT_PAREN) {
        do {
            ++subcompiler.function->arity;

            if (subcompiler.function->arity > 250) {
                risa_parser_error_at_current(subcompiler.parser, "Parameter limit exceeded (250)");
            }

            risa_compiler_declare_variable(&subcompiler);
            subcompiler.locals[subcompiler.localCount - 1].depth = subcompiler.scopeDepth;

            // Reserve a register for the parameter.
            ++subcompiler.regIndex;

            if(subcompiler.parser->current.type != RISA_TOKEN_COMMA)
                break;

            risa_parser_advance(subcompiler.parser);
        } while(true);
    }

    risa_parser_consume(subcompiler.parser, RISA_TOKEN_RIGHT_PAREN, "Expected ')' after lambda parameters");
    risa_parser_consume(subcompiler.parser, RISA_TOKEN_EQUAL_GREATER, "Expected '=>' after lambda parameters");

    if(subcompiler.parser->current.type == RISA_TOKEN_LEFT_BRACE) {
        risa_parser_advance(subcompiler.parser);
        risa_compiler_compile_block(&subcompiler);
        risa_compiler_emit_return(&subcompiler);
    } else risa_compiler_compile_return_expression(&subcompiler);

    if(!risa_compiler_register_reserve(compiler))
        return;

    if(subcompiler.upvalueCount == 0) {
        risa_compiler_emit_constant(compiler, RISA_DENSE_VALUE(subcompiler.function));
    } else {
        risa_compiler_emit_constant(compiler, RISA_DENSE_VALUE(subcompiler.function));

        risa_compiler_emit_byte(compiler, RISA_OP_CLSR);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_byte(compiler, subcompiler.upvalueCount);

        for(uint8_t i = 0; i < subcompiler.upvalueCount; ++i) {
            risa_compiler_emit_byte(compiler, RISA_OP_UPVAL);
            risa_compiler_emit_byte(compiler, subcompiler.upvalues[i].index);
            risa_compiler_emit_byte(compiler, subcompiler.upvalues[i].local);
            risa_compiler_emit_byte(compiler, 0);
        }
    }

    compiler->last.reg = compiler->regIndex - 1;
    compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_CONSTANT, {RISA_TOKEN_IDENTIFIER, "lambda", 6 } };
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = true;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    risa_compiler_delete(&subcompiler);
}

static void risa_compiler_compile_accessor(RisaCompiler* compiler, bool allowAssignment) {
    uint8_t leftReg = compiler->last.reg;
    bool isLeftNew = compiler->last.isNew;
    bool isLeftConst = compiler->last.isConst;
    bool isLeftOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_compile_expression(compiler);

    bool isLength = (compiler->parser->previous.type == RISA_TOKEN_STRING && compiler->parser->previous.size == 8 && memcmp(compiler->parser->previous.start, "\"length\"", sizeof("\"length\"") - 1) == 0);

    risa_parser_consume(compiler->parser, RISA_TOKEN_RIGHT_BRACKET, "Expected ']' after expression");

    bool isRightOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    if(allowAssignment && (compiler->parser->current.type == RISA_TOKEN_EQUAL)) {
        if(isLength) {
            risa_parser_error_at_previous(compiler->parser, "Cannot assign to length");
            return;
        }

        uint8_t rightReg = compiler->last.reg;
        bool isRightConst = compiler->last.isConst;
        bool isRightNew = compiler->last.isNew;

        risa_parser_advance(compiler->parser);
        risa_compiler_compile_expression(compiler);

        compiler->last.isConstOptimized = risa_compiler_can_optimize_last_cnst(compiler);

        // TODO: Test this for all cases.
        risa_compiler_optimize_last_cnst(compiler);

        #define LR_TYPES (isRightOptimized * RISA_TODLR_TYPE_LEFT_MASK) | (compiler->last.isConstOptimized * RISA_TODLR_TYPE_RIGHT_MASK)

        risa_compiler_emit_byte(compiler, RISA_OP_SET | LR_TYPES);
        risa_compiler_emit_byte(compiler, leftReg);
        risa_compiler_emit_byte(compiler, rightReg);
        risa_compiler_emit_byte(compiler, compiler->last.reg);

        #undef LR_TYPES

        if(isLeftNew)
            risa_compiler_register_free(compiler);
        if(isRightNew)
            risa_compiler_register_free(compiler);
        if(!compiler->last.canOverwrite || compiler->last.lvalMeta.type == LVAL_GLOBAL)
            risa_compiler_register_free(compiler);

        compiler->last.reg = leftReg;
        compiler->last.isNew = true;
        compiler->last.isConst = false;
        compiler->last.isLvalue = false;
        compiler->last.fromBranched = false;
    } else {
        uint8_t destReg;

        if(compiler->last.isNew) {
            destReg = compiler->last.reg;

            if(isLeftNew)
                risa_compiler_register_free(compiler);
        } else if(compiler->last.canOverwrite && isLeftNew) {
            destReg = leftReg;
        } else {
            risa_compiler_register_reserve(compiler);
            destReg = compiler->regIndex - 1;
        }

        #define R_TYPE (isRightOptimized * RISA_TODLR_TYPE_RIGHT_MASK)

        risa_compiler_emit_byte(compiler, isLength ? RISA_OP_LEN : RISA_OP_GET | R_TYPE);
        risa_compiler_emit_byte(compiler, destReg);
        risa_compiler_emit_byte(compiler, leftReg);
        risa_compiler_emit_byte(compiler, isLength ? 0 : compiler->last.reg);

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
        compiler->last.fromBranched = false;

        compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_TEMP };
    }
}

static void risa_compiler_compile_unary(RisaCompiler* compiler, bool allowAssignment) {
    RisaTokenType operator = compiler->parser->previous.type;

    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_UNARY);

    uint8_t destReg;

    // TODO: use risa_compiler_optimize_last_cnst here
    if(compiler->last.isConst) {
        //risa_compiler_register_free(compiler);
        destReg = compiler->last.reg;
        compiler->last.reg = compiler->function->cluster.bytecode[compiler->function->cluster.size - 2];
        compiler->function->cluster.size -= 4;
    } else if(compiler->last.isNew) {
        destReg = compiler->last.reg;
    } else if(compiler->regs[compiler->last.reg].type != RISA_REG_TEMP) {
        if(!risa_compiler_register_reserve(compiler))
            return;
        destReg = compiler->regIndex - 1;
    } else destReg = compiler->last.reg;

    #define L_TYPE (compiler->last.isConst * RISA_TODLR_TYPE_LEFT_MASK)

    switch(operator) {
        case RISA_TOKEN_BANG:
            risa_compiler_emit_bytes(compiler, RISA_OP_NOT | L_TYPE, destReg, compiler->last.reg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case RISA_TOKEN_TILDE:
            risa_compiler_emit_bytes(compiler, RISA_OP_BNOT | L_TYPE, destReg, compiler->last.reg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case RISA_TOKEN_MINUS:
            risa_compiler_emit_bytes(compiler, RISA_OP_NEG | L_TYPE, destReg, compiler->last.reg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    compiler->last.reg = destReg;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    compiler->regs[destReg] = (RisaRegInfo) {RISA_REG_TEMP };

    #undef L_TYPE
}

static void risa_compiler_compile_binary(RisaCompiler* compiler, bool allowAssignment) {
    bool isLeftOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    uint8_t leftReg = compiler->last.reg;
    bool isLeftNew = compiler->last.isNew;
    bool isLeftConst = compiler->last.isConst;

    // TODO: check if this works exactly the same
    /*if(isLeftConst) {
        risa_compiler_register_free(compiler);
        leftReg = compiler->function->cluster.bytecode[compiler->function->cluster.size - 2];
        compiler->function->cluster.size -= 4;
        isLeftNew = false;
    }*/

    RisaTokenType operatorType = compiler->parser->previous.type;

    const RisaOperatorRule* rule = &OPERATOR_RULES[operatorType];
    risa_compiler_compile_expression_precedence(compiler, (RisaOperatorPrecedence) (rule->precedence + 1));

    bool isRightOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    // The GT and GTE instructions are simulated with reversed LT and LTE. Therefore, switch the operands and use the REV def.
    #define LR_TYPES ((isLeftOptimized * RISA_TODLR_TYPE_LEFT_MASK) | (isRightOptimized * RISA_TODLR_TYPE_RIGHT_MASK))
    #define LR_TYPES_REV ((isRightOptimized * RISA_TODLR_TYPE_LEFT_MASK) | (isLeftOptimized * RISA_TODLR_TYPE_RIGHT_MASK))

    switch(operatorType) {
        case RISA_TOKEN_PLUS:
            risa_compiler_emit_byte(compiler, RISA_OP_ADD | LR_TYPES);
            break;
        case RISA_TOKEN_MINUS:
            risa_compiler_emit_byte(compiler, RISA_OP_SUB | LR_TYPES);
            break;
        case RISA_TOKEN_STAR:
            risa_compiler_emit_byte(compiler, RISA_OP_MUL | LR_TYPES);
            break;
        case RISA_TOKEN_SLASH:
            risa_compiler_emit_byte(compiler, RISA_OP_DIV | LR_TYPES);
            break;
        case RISA_TOKEN_PERCENT:
            risa_compiler_emit_byte(compiler, RISA_OP_MOD | LR_TYPES);
            break;
        case RISA_TOKEN_LESS_LESS:
            risa_compiler_emit_byte(compiler, RISA_OP_SHL | LR_TYPES);
            break;
        case RISA_TOKEN_GREATER_GREATER:
            risa_compiler_emit_byte(compiler, RISA_OP_SHR | LR_TYPES);
            break;
        case RISA_TOKEN_GREATER:
            risa_compiler_emit_byte(compiler, RISA_OP_LT | LR_TYPES_REV);
            break;
        case RISA_TOKEN_GREATER_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_LTE | LR_TYPES_REV);
            break;
        case RISA_TOKEN_LESS:
            risa_compiler_emit_byte(compiler, RISA_OP_LT | LR_TYPES);
            break;
        case RISA_TOKEN_LESS_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_LTE | LR_TYPES);
            break;
        case RISA_TOKEN_EQUAL_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_EQ | LR_TYPES);
            break;
        case RISA_TOKEN_BANG_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_NEQ | LR_TYPES);
            break;
        case RISA_TOKEN_AMPERSAND:
            risa_compiler_emit_byte(compiler, RISA_OP_BAND | LR_TYPES);
            break;
        case RISA_TOKEN_CARET:
            risa_compiler_emit_byte(compiler, RISA_OP_BXOR | LR_TYPES);
            break;
        case RISA_TOKEN_PIPE:
            risa_compiler_emit_byte(compiler, RISA_OP_BOR | LR_TYPES);
            break;
        default:
            return;
    }

    uint8_t destReg;

    if(compiler->last.isNew/*compiler->regs[compiler->last.reg].type == RISA_REG_TEMP*/) {
        destReg = compiler->last.reg;

        if(isLeftNew) { // TODO: check if this works for all cases.
            destReg = leftReg;
            risa_compiler_register_free(compiler);
        }
    } else if(isLeftNew/*compiler->regs[leftReg].type == RISA_REG_TEMP*/) {
        destReg = leftReg;
    } else {
        risa_compiler_register_reserve(compiler);
        destReg = compiler->regIndex - 1;
    }

    // Reverse in the case of < and <= because only LT and LTE exist.
    if(operatorType == RISA_TOKEN_GREATER || operatorType == RISA_TOKEN_GREATER_EQUAL)
        risa_compiler_emit_bytes(compiler, destReg, compiler->last.reg, leftReg);
    else risa_compiler_emit_bytes(compiler, destReg, leftReg, compiler->last.reg);

    compiler->regs[destReg] = (RisaRegInfo) {RISA_REG_TEMP };
    compiler->last.reg = destReg;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    #undef LR_TYPES
}

static void risa_compiler_compile_ternary(RisaCompiler* compiler, bool allowAssignment) {
    risa_compiler_emit_byte(compiler, RISA_OP_TEST);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    risa_compiler_register_free(compiler);

    uint32_t first = risa_compiler_emit_blank(compiler);

    risa_compiler_compile_expression(compiler);
    risa_parser_consume(compiler->parser, RISA_TOKEN_COLON, "Expected ':' after ternary operator expression");
    risa_compiler_register_free(compiler);

    uint32_t second = risa_compiler_emit_blank(compiler);

    risa_compiler_emit_jump(compiler, first);
    risa_compiler_compile_expression(compiler);
    risa_compiler_emit_jump(compiler, second);

    // Disable CNST optimizations that can be bad (e.g. if the next instruction is RISA_OP_SET, it will only take the last branch value)
    compiler->last.fromBranched = true;
}

static void risa_compiler_compile_equal_op(RisaCompiler* compiler, bool allowAssignment) {
    RisaTokenType operator = compiler->parser->previous.type;

    if(!compiler->last.isLvalue) {
        risa_parser_error_at_current(compiler->parser, "Cannot assign to non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    bool isNew = compiler->last.isNew;
    bool isConst = compiler->last.isConst;
    bool isLvalue = compiler->last.isLvalue;

    // Save the lval meta.
    uint8_t meta[sizeof(compiler->last.lvalMeta)];
    memcpy(meta, &compiler->last.lvalMeta, sizeof(compiler->last.lvalMeta));

    risa_compiler_compile_expression(compiler);

    bool isOptimized = risa_compiler_can_optimize_last_cnst(compiler);

    risa_compiler_optimize_last_cnst(compiler);

    #define R_TYPE (isOptimized * RISA_TODLR_TYPE_RIGHT_MASK)

    switch(operator) {
        case RISA_TOKEN_PLUS_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_ADD | R_TYPE);
            break;
        case RISA_TOKEN_MINUS_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_SUB | R_TYPE);
            break;
        case RISA_TOKEN_STAR_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_MUL | R_TYPE);
            break;
        case RISA_TOKEN_SLASH_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_DIV | R_TYPE);
            break;
        case RISA_TOKEN_CARET_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_BXOR | R_TYPE);
            break;
        case RISA_TOKEN_PERCENT_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_MOD | R_TYPE);
            break;
        case RISA_TOKEN_PIPE_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_BOR | R_TYPE);
            break;
        case RISA_TOKEN_AMPERSAND_EQUAL:
            risa_compiler_emit_byte(compiler, RISA_OP_BAND | R_TYPE);
            break;
        default:
            return;
    }

    risa_compiler_emit_byte(compiler, destReg);
    risa_compiler_emit_byte(compiler, destReg);
    risa_compiler_emit_byte(compiler, compiler->last.reg);

    #undef R_TYPE

    // Load the lval meta.
    memcpy(&compiler->last.lvalMeta, meta, sizeof(compiler->last.lvalMeta));

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * RISA_TODLR_TYPE_LEFT_MASK)

    // TODO: propagations may need to set compiler->last.isConstOptimized
    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            risa_compiler_emit_byte(compiler, 0);
        case LVAL_UPVAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    compiler->last.reg = destReg;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = isNew;
    compiler->last.isConst = isConst;
    compiler->last.isLvalue = isLvalue;
    compiler->last.isPostIncrement = false;
    compiler->last.isEqualOp = true;
    compiler->last.fromBranched = false;

    // Don't overwrite the reg info
    //compiler->regs[compiler->last.reg] = (RisaRegInfo) { RISA_REG_TEMP };
}

static void risa_compiler_compile_prefix(RisaCompiler* compiler, bool allowAssignment) {
    RisaTokenType operator = compiler->parser->previous.type;

    compiler->last.canOverwrite = false;

    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_UNARY);

    compiler->last.canOverwrite = false; // true

    if(!compiler->last.isLvalue) {
        risa_parser_error_at_current(compiler->parser, "Cannot increment non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    switch(operator) {
        case RISA_TOKEN_MINUS_MINUS:
            risa_compiler_emit_bytes(compiler, RISA_OP_DEC, destReg, 0);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case RISA_TOKEN_PLUS_PLUS:
            risa_compiler_emit_bytes(compiler, RISA_OP_INC, destReg, 0);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * RISA_TODLR_TYPE_LEFT_MASK)

    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    compiler->last.fromBranched = false;
    compiler->last.isConstOptimized = false;

    /* Currently, this will give you 13:
     *
     * var a = 5;
     * var b = ++a + a++;
     *
     * If you want to get a more natural response (12), uncomment this line.
     * However, doing so will disable some risa_compiler_register_find optimizations.
     * It's not really worth it.
     */
    // compiler->regs[compiler->last.reg] = (RisaRegInfo) { RISA_REG_TEMP };
}

static void risa_compiler_compile_postfix(RisaCompiler* compiler, bool allowAssignment) {
    RisaTokenType operator = compiler->parser->previous.type;

    if(!compiler->last.isLvalue) {
        risa_parser_error_at_current(compiler->parser, "Cannot increment non-lvalue");
        return;
    }

    uint8_t destReg = compiler->last.reg;

    if(!risa_compiler_register_reserve(compiler))
        return;

    risa_compiler_emit_bytes(compiler, RISA_OP_MOV, compiler->regIndex - 1, destReg);
    risa_compiler_emit_byte(compiler, 0);

    switch(operator) {
        case RISA_TOKEN_MINUS_MINUS:
            risa_compiler_emit_bytes(compiler, RISA_OP_DEC, destReg, 0);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case RISA_TOKEN_PLUS_PLUS:
            risa_compiler_emit_bytes(compiler, RISA_OP_INC, destReg, 0);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            return;
    }

    #define L_TYPE (compiler->last.lvalMeta.propIndex.isConst * RISA_TODLR_TYPE_LEFT_MASK)

    switch(compiler->last.lvalMeta.type) {
        case LVAL_LOCAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);
            break;
        case LVAL_GLOBAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, destReg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_GLOBAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SGLOB, compiler->last.lvalMeta.global, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL:
            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, destReg);
            risa_compiler_emit_byte(compiler, 0);
            break;
        case LVAL_UPVAL_PROP:
            risa_compiler_emit_bytes(compiler, RISA_OP_SET | L_TYPE, compiler->last.lvalMeta.propOrigin, compiler->last.lvalMeta.propIndex.isConst ? (uint8_t) compiler->last.lvalMeta.propIndex.as.cnst
                                                                                                                                                   : compiler->last.lvalMeta.propIndex.as.reg);
            risa_compiler_emit_byte(compiler, destReg);

            risa_compiler_emit_bytes(compiler, RISA_OP_SUPVAL, compiler->last.lvalMeta.upval, compiler->last.lvalMeta.propOrigin);
            risa_compiler_emit_byte(compiler, 0);
            break;
        default:
            break;
    }

    #undef L_TYPE

    compiler->last.reg = compiler->regIndex - 1;
    compiler->last.isConstOptimized = false;
    compiler->last.isNew = true;
    compiler->last.isConst = false;
    compiler->last.isLvalue = false;
    compiler->last.isPostIncrement = true;
    compiler->last.isEqualOp = false;
    compiler->last.fromBranched = false;

    compiler->regs[compiler->last.reg] = (RisaRegInfo) {RISA_REG_TEMP };
}

static void risa_compiler_compile_and(RisaCompiler* compiler, bool allowAssignment) {
    risa_compiler_emit_byte(compiler, RISA_OP_TEST);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    uint16_t index = risa_compiler_emit_blank(compiler);

    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_AND);

    risa_compiler_emit_jump(compiler, index);
}

static void risa_compiler_compile_or(RisaCompiler* compiler, bool allowAssignment) {
    risa_compiler_emit_byte(compiler, RISA_OP_NTEST);
    risa_compiler_emit_byte(compiler, compiler->last.reg);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    uint16_t index = risa_compiler_emit_blank(compiler);

    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_OR);

    risa_compiler_emit_jump(compiler, index);
}

static void risa_compiler_compile_comma(RisaCompiler* compiler, bool allowAssignment) {
    if(compiler->last.isNew)
        risa_compiler_register_free(compiler);

    risa_compiler_compile_expression_precedence(compiler, RISA_PREC_COMMA);
}

static void risa_compiler_scope_begin(RisaCompiler* compiler) {
    ++compiler->scopeDepth;
}

static void risa_compiler_scope_end(RisaCompiler* compiler) {
    --compiler->scopeDepth;

    while(compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        if(compiler->locals[compiler->localCount - 1].captured) {
            risa_compiler_emit_byte(compiler, RISA_OP_CUPVAL);
            risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
            risa_compiler_emit_byte(compiler, 0);
            risa_compiler_emit_byte(compiler, 0);
        }

        risa_compiler_register_free(compiler);
        --compiler->localCount;
    }
}

static void risa_compiler_local_add(RisaCompiler* compiler, RisaToken identifier) {
    if(compiler->localCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "RisaLocalInfo variable limit exceeded (250)");
        return;
    }

    //if(!risa_compiler_register_reserve(compiler))
    //    return;

    RisaLocalInfo* local = &compiler->locals[compiler->localCount++];
    local->identifier = identifier;
    local->depth = -1;
    local->reg = compiler->regIndex; // -1
    local->captured = false;

    compiler->regs[local->reg] = (RisaRegInfo) {RISA_REG_LOCAL, identifier };
}

static uint8_t risa_compiler_local_resolve(RisaCompiler* compiler, RisaToken* identifier) {
    for(int16_t i = compiler->localCount - 1; i >= 0; --i) {
        RisaLocalInfo* local = &compiler->locals[i];

        if(risa_token_identifier_equals(identifier, &local->identifier) && local->depth > -1)
            return local->reg;
    }

    return 251;
}

static uint8_t risa_compiler_upvalue_add(RisaCompiler* compiler, uint8_t index, bool local) {
    uint8_t upvalCount = compiler->upvalueCount;

    for(uint8_t i = 0; i < upvalCount; ++i) {
        RisaUpvalueInfo* upvalue = &compiler->upvalues[i];

        if(upvalue->index == index && upvalue->local == local)
            return i;
    }

    if(upvalCount == 250) {
        risa_parser_error_at_previous(compiler->parser, "Closure variable limit exceeded (250)");
    }

    compiler->upvalues[upvalCount].local = local;
    compiler->upvalues[upvalCount].index = index;
    return compiler->upvalueCount++;
}

static uint8_t risa_compiler_upvalue_resolve(RisaCompiler* compiler, RisaToken* identifier) {
    if(compiler->super == NULL)
        return 251;

    uint8_t local = risa_compiler_local_resolve(compiler->super, identifier);

    if(local != 251) {
        compiler->super->locals[local].captured = true;
        return risa_compiler_upvalue_add(compiler, local, true);
    }

    uint8_t upvalue = risa_compiler_upvalue_resolve(compiler->super, identifier);

    if(upvalue != 251)
        return risa_compiler_upvalue_add(compiler, upvalue, false);

    return 251;
}

static void risa_compiler_emit_byte(RisaCompiler* compiler, uint8_t byte) {
    risa_cluster_write(&compiler->function->cluster, byte, compiler->parser->previous.index);
}

static void risa_compiler_emit_bytes(RisaCompiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
    risa_compiler_emit_byte(compiler, byte1);
    risa_compiler_emit_byte(compiler, byte2);
    risa_compiler_emit_byte(compiler, byte3);
}

static void risa_compiler_emit_word(RisaCompiler* compiler, uint16_t word) {
    risa_compiler_emit_byte(compiler, ((uint8_t*) &word)[0]);
    risa_compiler_emit_byte(compiler, ((uint8_t*) &word)[1]);
}

static void risa_compiler_emit_constant(RisaCompiler* compiler, RisaValue value) {
    uint16_t index = risa_compiler_create_constant(compiler, value);

    if(index < UINT8_MAX) {
        risa_compiler_emit_bytes(compiler, RISA_OP_CNST, compiler->regIndex - 1, index);
        risa_compiler_emit_byte(compiler, 0);
    } else {
        risa_compiler_emit_byte(compiler, RISA_OP_CNSTW);
        risa_compiler_emit_byte(compiler, compiler->regIndex - 1);
        risa_compiler_emit_word(compiler, index);
    }
}

static void risa_compiler_emit_return(RisaCompiler* compiler) {
    risa_compiler_emit_byte(compiler, RISA_OP_RET);
    risa_compiler_emit_byte(compiler, 251);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);
}

static void risa_compiler_emit_mov(RisaCompiler* compiler, uint8_t dest, uint8_t src) {
    if(dest != src) {
        risa_compiler_emit_byte(compiler, RISA_OP_MOV);
        risa_compiler_emit_byte(compiler, dest);
        risa_compiler_emit_byte(compiler, src);
        risa_compiler_emit_byte(compiler, 0);
    }
}

static uint32_t risa_compiler_emit_blank(RisaCompiler* compiler) {
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);
    risa_compiler_emit_byte(compiler, 0);

    return compiler->function->cluster.size - 4;
}

static void risa_compiler_emit_jump(RisaCompiler* compiler, uint32_t index) {
    uint32_t diff = (compiler->function->cluster.size - index - 4) / 4;

    if(diff <= UINT8_MAX) {
        compiler->function->cluster.bytecode[index] = RISA_OP_JMP;
        compiler->function->cluster.bytecode[index + 1] = (uint8_t) diff;
    } else if(diff <= UINT16_MAX) {
        uint16_t word = (uint16_t) diff;

        compiler->function->cluster.bytecode[index] = RISA_OP_JMPW;
        compiler->function->cluster.bytecode[index + 1] = ((uint8_t*) &word)[0];
        compiler->function->cluster.bytecode[index + 2] = ((uint8_t*) &word)[1];
    } else {
        risa_parser_error_at_previous(compiler->parser, "Jump limit exceeded (65535)");
        return;
    }
}

static void risa_compiler_emit_backwards_jump(RisaCompiler* compiler, uint32_t to) {
    risa_compiler_emit_backwards_jump_from(compiler, compiler->function->cluster.size, to);
}

static void risa_compiler_emit_backwards_jump_from(RisaCompiler* compiler, uint32_t from, uint32_t to) {
    uint32_t diff = (from - to) / 4;

    if(diff <= UINT8_MAX) {
        if(from == compiler->function->cluster.size) {
            risa_compiler_emit_byte(compiler, RISA_OP_BJMP);
            risa_compiler_emit_byte(compiler, (uint8_t) diff);
            risa_compiler_emit_byte(compiler, 0);
            risa_compiler_emit_byte(compiler, 0);
        } else {
            compiler->function->cluster.bytecode[from] = RISA_OP_BJMP;
            compiler->function->cluster.bytecode[from + 1] = (uint8_t) diff;
        }
    } else if(diff <= UINT16_MAX) {
        uint16_t word = (uint16_t) diff;

        if(from == compiler->function->cluster.size) {
            risa_compiler_emit_byte(compiler, RISA_OP_BJMPW);
            risa_compiler_emit_word(compiler, word);
            risa_compiler_emit_byte(compiler, 0);
        }

        compiler->function->cluster.bytecode[from] = RISA_OP_BJMPW;
        compiler->function->cluster.bytecode[from + 1] = ((uint8_t*) &word)[0];
        compiler->function->cluster.bytecode[from + 2] = ((uint8_t*) &word)[1];
    } else {
        risa_parser_error_at_previous(compiler->parser, "Jump limit exceeded (65535)");
        return;
    }
}

static uint16_t risa_compiler_create_constant(RisaCompiler* compiler, RisaValue value) {
    size_t index = risa_cluster_write_constant(&compiler->function->cluster, value);

    if(index > UINT16_MAX) {
        risa_parser_error_at_previous(compiler->parser, "Constant limit exceeded (65535)");
        return 0;
    }

    return (uint16_t) index;
}

static uint16_t risa_compiler_create_identifier_constant(RisaCompiler* compiler) {
    return risa_compiler_create_string_constant(compiler, compiler->parser->previous.start, compiler->parser->previous.size);
}

static uint16_t risa_compiler_create_string_constant(RisaCompiler* compiler, const char* start, uint32_t length) {
    uint32_t hash = risa_map_hash(start, length);

    RisaCompiler* super = compiler->super == NULL ? compiler : compiler->super;

    while(super->super != NULL)
        super = super->super;

    RisaDenseString* interned = risa_map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = risa_dense_string_from(start, length);
        risa_map_set(&super->strings, interned, RISA_NULL_VALUE);
    }

    return risa_compiler_create_constant(compiler, RISA_DENSE_VALUE(interned));
}

static uint16_t risa_compiler_declare_variable(RisaCompiler* compiler) {
    risa_parser_consume(compiler->parser, RISA_TOKEN_IDENTIFIER, "Expected identifier");

    if(compiler->scopeDepth > 0) {
        RisaToken* identifier = &compiler->parser->previous;

        for(int16_t i = compiler->localCount - 1; i >= 0; --i) {
            RisaLocalInfo* local = &compiler->locals[i];

            if(local->depth != -1 && local->depth < compiler->scopeDepth)
                break;

            if(risa_token_identifier_equals(identifier, &local->identifier)) {
                risa_parser_error_at_previous(compiler->parser, "Variable already declared in this scope");
                return -1;
            }
        }

        risa_compiler_local_add(compiler, *identifier);
        return 0;
    }

    return risa_compiler_create_identifier_constant(compiler);
}

static bool risa_compiler_can_optimize_last_cnst(RisaCompiler* compiler) {
    // isConst, has at least one instruction in the cluster, the instruction is CNST, and not from branched.
    // Note: when isConst is true, the last instruction is not always CNST (e.g. it can be UPVAL, when closing a function with CLSR)
    // because CLSR generates a closure which is constant.

    return compiler->last.isConst
        && compiler->function->cluster.size >= 4
        && compiler->function->cluster.bytecode[compiler->function->cluster.size - 4] == RISA_OP_CNST
        && !compiler->last.fromBranched;
}

static void risa_compiler_optimize_last_cnst(RisaCompiler* compiler) {
    // TODO: make this function return bool (true if optimized, false otherwise).
    if(risa_compiler_can_optimize_last_cnst(compiler)) {
        risa_compiler_register_free(compiler);
        compiler->last.reg = compiler->function->cluster.bytecode[compiler->function->cluster.size - 2];
        compiler->last.isConstOptimized = true;
        compiler->last.isNew = false;
        compiler->function->cluster.size -= 4;
    }
}

static bool risa_compiler_register_reserve(RisaCompiler* compiler) {
    if(compiler->regIndex == 249) {
        risa_parser_error_at_current(compiler->parser, "Register limit exceeded (250)");
        return false;
    }
    else {
        compiler->regs[compiler->regIndex] = (RisaRegInfo) {RISA_REG_TEMP };
        ++compiler->regIndex;
        return true;
    }
}

static uint8_t risa_compiler_register_find(RisaCompiler* compiler, RisaRegType type, RisaToken token) {
    if(compiler->regIndex == 0)
        return 251;

    for(int16_t i = compiler->regIndex - 1; i >= 0; --i)
        if(compiler->regs[i].type == type)
            if(compiler->regs[i].token.size == token.size && memcmp(compiler->regs[i].token.start, token.start, token.size) == 0)
                return i;
    return 251;
}

static void risa_compiler_register_free(RisaCompiler* compiler) {
    --compiler->regIndex;
}

static void risa_compiler_finalize_compilation(RisaCompiler* compiler) {
    risa_compiler_emit_return(compiler);
}