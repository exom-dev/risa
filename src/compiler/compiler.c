#include "compiler.h"
#include "parser.h"

#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../lexer/lexer.h"

#include <stdlib.h>
#include <errno.h>

static void compile_byte(Compiler* compiler, bool);
static void compile_int(Compiler* compiler, bool);
static void compile_float(Compiler* compiler, bool);
static void compile_string(Compiler* compiler, bool);
static void compile_literal(Compiler* compiler, bool);
static void compile_identifier(Compiler* compiler, bool);
static void compile_declaration(Compiler* compiler);
static void compile_variable_declaration(Compiler* compiler);
static void compile_statement(Compiler* compiler);
static void compile_expression_statement(Compiler* compiler);
static void compile_expression(Compiler* compiler);
static void compile_expression_precedence(Compiler* compiler, Precedence precedence);
static void compile_grouping(Compiler* compiler, bool);
static void compile_unary(Compiler* compiler, bool);
static void compile_binary(Compiler* compiler, bool);

static void emit_byte(Compiler* compiler, uint8_t byte);
static void emit_bytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3);
static void emit_word(Compiler* compiler, uint16_t word);
static void emit_constant(Compiler* compiler, Value value);
static void emit_return(Compiler* compiler);

static uint16_t create_constant(Compiler* compiler, Value value);
static uint16_t create_identifier_constant(Compiler* compiler);
static uint16_t create_string_constant(Compiler* compiler, const char* start, uint32_t length);
static uint16_t get_variable_name(Compiler* compiler);

static bool register_reserve(Compiler* compiler);
static void register_free(Compiler* compiler);

static void finalize_compilation(Compiler* compiler);

Rule EXPRESSION_RULES[] = {
        { compile_grouping, NULL,    PREC_NONE },        // TOKEN_LEFT_PAREN
        { NULL,     NULL,    PREC_NONE },                // TOKEN_RIGHT_PAREN
        { NULL,     NULL,    PREC_NONE },                // TOKEN_LEFT_BRACKET
        { NULL,     NULL,    PREC_NONE },                // TOKEN_RIGHT_BRACKET
        { NULL,     NULL,    PREC_NONE },                // TOKEN_LEFT_BRACE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_RIGHT_BRACE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_COMMA
        { NULL,     NULL,    PREC_NONE },                // TOKEN_DOT
        { compile_unary,    compile_binary,  PREC_TERM },// TOKEN_MINUS
        { NULL,     compile_binary,  PREC_TERM },        // TOKEN_PLUS
        { NULL,     NULL,    PREC_NONE },                // TOKEN_COLON
        { NULL,     NULL,  PREC_NONE },                  // TOKEN_SEMICOLON
        { NULL,     compile_binary,  PREC_FACTOR },      // TOKEN_SLASH
        { NULL,     compile_binary,    PREC_FACTOR },    // TOKEN_STAR
        { compile_unary,     NULL,    PREC_NONE },       // TOKEN_TILDE
        { NULL,     compile_binary,    PREC_BITWISE_XOR },// TOKEN_AMPERSAND
        { NULL,     compile_binary,    PREC_FACTOR },     // TOKEN_PERCENT
        { compile_unary,     NULL,    PREC_NONE },       // TOKEN_BANG
        { NULL,     compile_binary,    PREC_EQUALITY },  // TOKEN_BANG_EQUAL
        { NULL,     NULL,    PREC_NONE },                // TOKEN_EQUAL
        { NULL,     compile_binary,    PREC_EQUALITY },  // TOKEN_EQUAL_EQUAL
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_GREATER
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_GREATER_EQUAL
        { NULL,     compile_binary,    PREC_SHIFT },     // TOKEN_GREATER_GREATER
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_LESS
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_LESS_EQUAL
        { NULL,     compile_binary,    PREC_SHIFT },     // TOKEN_LESS_LESS
        { NULL,     compile_binary,    PREC_BITWISE_AND },// TOKEN_AMPERSAND
        { NULL,     NULL,    PREC_NONE },                // TOKEN_AMPERSAND_AMPERSAND
        { NULL,     compile_binary,    PREC_BITWISE_OR },// TOKEN_PIPE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_PIPE_PIPE
        { compile_identifier,     NULL,    PREC_NONE },  // TOKEN_IDENTIFIER
        { compile_string,     NULL,    PREC_NONE },      // TOKEN_STRING
        { compile_byte,     NULL,    PREC_NONE },        // TOKEN_BYTE
        { compile_int,     NULL,    PREC_NONE },         // TOKEN_INT
        { compile_float,     NULL,    PREC_NONE },       // TOKEN_FLOAT
        { NULL,     NULL,    PREC_NONE },                // TOKEN_IF
        { NULL,     NULL,    PREC_NONE },                // TOKEN_ELSE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_WHILE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_FOR
        { compile_literal,     NULL,    PREC_NONE },     // TOKEN_TRUE
        { compile_literal,     NULL,    PREC_NONE },     // TOKEN_FALSE
        { compile_literal,     NULL,    PREC_NONE },     // TOKEN_NULL
        { NULL,     NULL,    PREC_NONE },                // TOKEN_VAR
        { NULL,     NULL,    PREC_NONE },                // TOKEN_FUNCTION
        { NULL,     NULL,    PREC_NONE },                // TOKEN_RETURN
        { NULL,     NULL,    PREC_NONE },                // TOKEN_ERROR
        { NULL,     NULL,    PREC_NONE },                // TOKEN_EOF
};

void compiler_init(Compiler* compiler) {
    chunk_init(&compiler->chunk);
    parser_init(&compiler->parser);
    map_init(&compiler->strings);

    compiler->regIndex = 0;
}

void compiler_delete(Compiler* compiler) {
    map_delete(&compiler->strings);
}

CompilerStatus compiler_compile(Compiler* compiler, const char* str) {
    parser_init(&compiler->parser);
    lexer_init(&compiler->parser.lexer);
    lexer_source(&compiler->parser.lexer, str);

    parser_advance(&compiler->parser);

    while(compiler->parser.current.type != TOKEN_EOF) {
        compile_declaration(compiler);
    }

    finalize_compilation(compiler);

    return compiler->parser.error ? COMPILER_ERROR : COMPILER_OK;
}

static void compile_byte(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    int64_t num = strtol(compiler->parser.previous.start, NULL, 10);

    if(errno == ERANGE || num > 256) {
        parser_error_at_previous(&compiler->parser, "Number is too large for type 'byte'");
        return;
    }

    emit_constant(compiler, BYTE_VALUE(num));
}

static void compile_int(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    int64_t num = strtol(compiler->parser.previous.start, NULL, 10);

    if(errno == ERANGE) {
        parser_error_at_previous(&compiler->parser, "Number is too large for type 'int'");
        return;
    }

    emit_constant(compiler, INT_VALUE(num));
}

static void compile_float(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    double num = strtod(compiler->parser.previous.start, NULL);

    if(errno == ERANGE) {
        parser_error_at_previous(&compiler->parser, "Number is too small or too large for type 'float'");
        return;
    }

    emit_constant(compiler, FLOAT_VALUE(num));
}

static void compile_string(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    const char* start = compiler->parser.previous.start + 1;
    uint32_t length = compiler->parser.previous.size - 2;
    uint32_t hash = map_hash(start, length);

    ValString* interned = map_find(&compiler->strings, start, length, hash);

    if(interned == NULL) {
        interned = value_string_from(start, length);
        map_set(&compiler->strings, interned, NULL_VALUE);
    }

    emit_constant(compiler, LINKED_VALUE(interned));
}

static void compile_literal(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    switch(compiler->parser.previous.type) {
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
}

static void compile_identifier(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    uint16_t index = create_identifier_constant(compiler);

    if(allowAssignment && (compiler->parser.current.type == TOKEN_EQUAL)) {
        parser_advance(&compiler->parser);
        compile_expression(compiler);

        emit_byte(compiler, OP_SGLOB);
        emit_byte(compiler, compiler->regIndex);

        register_free(compiler);
    } else {
        emit_byte(compiler, OP_GGLOB);
        emit_byte(compiler, compiler->regIndex - 1);
    }

    emit_byte(compiler, index);
}

static void compile_declaration(Compiler* compiler) {
    if(compiler->parser.current.type == TOKEN_VAR) {
        parser_advance(&compiler->parser);
        compile_variable_declaration(compiler);
    } else compile_statement(compiler);

    if(compiler->parser.panic)
        parser_sync(&compiler->parser);
}

static void compile_variable_declaration(Compiler* compiler) {
    uint16_t index = get_variable_name(compiler);

    if(compiler->parser.current.type == TOKEN_EQUAL) {
        parser_advance(&compiler->parser);
        compile_expression(compiler);
    } else {
        if(!register_reserve(compiler))
            return;

        emit_byte(compiler, OP_NULL);
        emit_byte(compiler, compiler->regIndex - 1);
    }

    register_free(compiler);
    parser_consume(&compiler->parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration");

    emit_byte(compiler, OP_DGLOB);
    emit_byte(compiler, compiler->regIndex);
    emit_byte(compiler, index);
}

static void compile_statement(Compiler* compiler) {
    compile_expression_statement(compiler);
}

static void compile_expression_statement(Compiler* compiler) {
    compile_expression(compiler);

    parser_consume(&compiler->parser, TOKEN_SEMICOLON, "Expected ';' after expression");

    register_free(compiler);
}

static void compile_expression(Compiler* compiler) {
    compile_expression_precedence(compiler, PREC_ASSIGNMENT);
}

static void compile_expression_precedence(Compiler* compiler, Precedence precedence) {
    parser_advance(&compiler->parser);

    RuleHandler prefix = EXPRESSION_RULES[compiler->parser.previous.type].prefix;

    if(prefix == NULL) {
        parser_error_at_previous(&compiler->parser, "Expected expression");
    }

    bool allowAssignment = precedence <= PREC_ASSIGNMENT;
    prefix(compiler, allowAssignment);

    while(precedence <= EXPRESSION_RULES[compiler->parser.current.type].precedence) {
        parser_advance(&compiler->parser);

        RuleHandler infix = EXPRESSION_RULES[compiler->parser.previous.type].infix;
        infix(compiler, allowAssignment);
    }

    if(allowAssignment && (compiler->parser.current.type == TOKEN_EQUAL)) {
        parser_error_at_previous(&compiler->parser, "Invalid assignment target");
    }
}

static void compile_grouping(Compiler* compiler, bool allowAssignment) {
    compile_expression(compiler);
    parser_consume(&compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void compile_unary(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser.previous.type;

    compile_expression_precedence(compiler, PREC_UNARY);

    switch(operator) {
        case TOKEN_BANG:
            emit_bytes(compiler, OP_NOT, compiler->regIndex - 1, compiler->regIndex - 1);
            break;
        case TOKEN_TILDE:
            emit_bytes(compiler, OP_BNOT, compiler->regIndex - 1, compiler->regIndex - 1);
            break;
        case TOKEN_MINUS:
            emit_bytes(compiler, OP_NEG, compiler->regIndex - 1, compiler->regIndex - 1);
            break;
        default:
            return;
    }
}

static void compile_binary(Compiler* compiler, bool allowAssignment) {
    TokenType operatorType = compiler->parser.previous.type;

    Rule* rule = &EXPRESSION_RULES[operatorType];
    compile_expression_precedence(compiler, (Precedence) (rule->precedence + 1));

    switch(operatorType) {
        case TOKEN_PLUS:
            emit_byte(compiler, OP_ADD);
            break;
        case TOKEN_MINUS:
            emit_byte(compiler, OP_SUB);
            break;
        case TOKEN_STAR:
            emit_byte(compiler, OP_MUL);
            break;
        case TOKEN_SLASH:
            emit_byte(compiler, OP_DIV);
            break;
        case TOKEN_PERCENT:
            emit_byte(compiler, OP_MOD);
            break;
        case TOKEN_LESS_LESS:
            emit_byte(compiler, OP_SHL);
            break;
        case TOKEN_GREATER_GREATER:
            emit_byte(compiler, OP_SHR);
            break;
        case TOKEN_GREATER:
            emit_byte(compiler, OP_GT);
            break;
        case TOKEN_GREATER_EQUAL:
            emit_byte(compiler, OP_GTE);
            break;
        case TOKEN_LESS:
            emit_byte(compiler, OP_LT);
            break;
        case TOKEN_LESS_EQUAL:
            emit_byte(compiler, OP_LTE);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_byte(compiler, OP_EQ);
            break;
        case TOKEN_BANG_EQUAL:
            emit_byte(compiler, OP_NEQ);
            break;
        case TOKEN_AMPERSAND:
            emit_byte(compiler, OP_BAND);
            break;
        case TOKEN_CARET:
            emit_byte(compiler, OP_BXOR);
            break;
        case TOKEN_PIPE:
            emit_byte(compiler, OP_BOR);
            break;
        default:
            return;
    }

    register_free(compiler);
    emit_bytes(compiler, compiler->regIndex - 1, compiler->regIndex - 1, compiler->regIndex);
}

static void emit_byte(Compiler* compiler, uint8_t byte) {
    chunk_write(&compiler->chunk, byte, compiler->parser.previous.index);
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

    if(index < UINT8_MAX)
        emit_bytes(compiler, OP_CNST, compiler->regIndex - 1, index);
    else {
        emit_byte(compiler, OP_CNSTW);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_word(compiler, index);
    }
}

static void emit_return(Compiler* compiler) {
    emit_byte(compiler, OP_RET);
}

static uint16_t create_constant(Compiler* compiler, Value value) {
    size_t index = chunk_write_constant(&compiler->chunk, value);

    if(index > UINT16_MAX) {
        parser_error_at_previous(&compiler->parser, "Constant limit exceeded (65535)");
        return 0;
    }

    return (uint16_t) index;
}

static uint16_t create_identifier_constant(Compiler* compiler) {
    return create_string_constant(compiler, compiler->parser.previous.start, compiler->parser.previous.size);
}

static uint16_t create_string_constant(Compiler* compiler, const char* start, uint32_t length) {
    uint32_t hash = map_hash(start, length);

    ValString* interned = map_find(&compiler->strings, start, length, hash);

    if(interned == NULL) {
        interned = value_string_from(start, length);
        map_set(&compiler->strings, interned, NULL_VALUE);
    }

    return create_constant(compiler, LINKED_VALUE(interned));
}

static uint16_t get_variable_name(Compiler* compiler) {
    parser_consume(&compiler->parser, TOKEN_IDENTIFIER, "Expected identifier");

    return create_identifier_constant(compiler);
}

static bool register_reserve(Compiler* compiler) {
    if(compiler->regIndex == 249) {
        parser_error_at_current(&compiler->parser, "Register limit exceeded (250)");
        return false;
    }
    else {
        ++compiler->regIndex;
        return true;
    }
}

static void register_free(Compiler* compiler) {
    --compiler->regIndex;
}

static void finalize_compilation(Compiler* compiler) {
    emit_return(compiler);
}