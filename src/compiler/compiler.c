#include "compiler.h"
#include "parser.h"

#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../lexer/lexer.h"

#include <stdlib.h>

void compile_number(Compiler* compiler);
void compile_expression(Compiler* compiler);
void compile_expression_precedence(Compiler* compiler, Precedence precedence);
void compile_grouping(Compiler* compiler);
void compile_unary(Compiler* compiler);
void compile_binary(Compiler* compiler);

void emit_byte(Compiler* compiler, uint8_t byte);
void emit_bytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3);
void emit_word(Compiler* compiler, uint16_t word);
void emit_constant(Compiler* compiler, Value value);
void emit_return(Compiler* compiler);

uint8_t create_constant(Compiler* compiler, Value value);

bool register_reserve(Compiler* compiler);
void register_free(Compiler* compiler);

void finalize_compilation(Compiler* compiler);

Rule EXPRESSION_RULES[] = {
        { compile_grouping, NULL,    PREC_NONE },       // TOKEN_LEFT_PAREN
        { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
        { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACKET
        { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACKET
        { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
        { NULL,     NULL,    PREC_NONE },       // TOKEN_DOT
        { compile_unary,    compile_binary,  PREC_TERM },       // TOKEN_MINUS
        { NULL,     compile_binary,  PREC_TERM },       // TOKEN_PLUS
        { NULL,     NULL,    PREC_NONE },       // TOKEN_COLON
        { NULL,     NULL,  PREC_NONE },     // TOKEN_SEMICOLON
        { NULL,     compile_binary,  PREC_FACTOR },     // TOKEN_SLASH
        { NULL,     compile_binary,    PREC_FACTOR },       // TOKEN_STAR
        { NULL,     NULL,    PREC_NONE },       // TOKEN_TILDE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_PERCENT
        { NULL,     NULL,    PREC_NONE },       // TOKEN_BANG
        { NULL,     NULL,    PREC_NONE },       // TOKEN_BANG_EQUAL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL_EQUAL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_GREATER
        { NULL,     NULL,    PREC_NONE },       // TOKEN_GREATER_EQUAL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_GREATER_GREATER
        { NULL,     NULL,    PREC_NONE },       // TOKEN_LESS
        { NULL,     NULL,    PREC_NONE },       // TOKEN_LESS_EQUAL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_LESS_LESS
        { NULL,     NULL,    PREC_NONE },       // TOKEN_AMPERSAND
        { NULL,     NULL,    PREC_NONE },       // TOKEN_AMPERSAND_AMPERSAND
        { NULL,     NULL,    PREC_NONE },       // TOKEN_PIPE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_PIPE_PIPE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
        { NULL,     NULL,    PREC_NONE },       // TOKEN_STRING
        { compile_number,     NULL,    PREC_NONE },       // TOKEN_NUMBER
        { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
        { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
        { NULL,     NULL,    PREC_NONE },       // TOKEN_TRUE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_FALSE
        { NULL,     NULL,    PREC_NONE },       // TOKEN_NULL
        { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR
        { NULL,     NULL,    PREC_NONE },       // TOKEN_FUNCTION
        { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
        { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR
        { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF
};

void compiler_init(Compiler* compiler) {
    chunk_init(&compiler->chunk);
    parser_init(&compiler->parser);

    compiler->regIndex = 0;
}

void compiler_delete(Compiler* compiler) {
    chunk_delete(&compiler->chunk);
}

CompilerStatus compiler_compile(Compiler* compiler, const char* str) {
    parser_init(&compiler->parser);
    lexer_init(&compiler->parser.lexer);
    lexer_source(&compiler->parser.lexer, str);

    parser_advance(&compiler->parser);
    compile_expression(compiler);

    parser_consume(&compiler->parser, TOKEN_EOF, "Expected end of expression");

    finalize_compilation(compiler);

    return compiler->parser.error ? COMPILER_ERROR : COMPILER_OK;
}

void compile_number(Compiler* compiler) {
    if(!register_reserve(compiler))
        return;

    double num = strtod(compiler->parser.previous.start, NULL);
    emit_constant(compiler, num);
}

void compile_expression(Compiler* compiler) {
    compile_expression_precedence(compiler, PREC_ASSIGNMENT);
}

void compile_expression_precedence(Compiler* compiler, Precedence precedence) {
    parser_advance(&compiler->parser);

    RuleHandler prefix = EXPRESSION_RULES[compiler->parser.previous.type].prefix;

    if(prefix == NULL) {
        parser_error_at_previous(&compiler->parser, "Expected expression");
    }

    prefix(compiler);

    while(precedence <= EXPRESSION_RULES[compiler->parser.current.type].precedence) {
        parser_advance(&compiler->parser);

        RuleHandler infix = EXPRESSION_RULES[compiler->parser.previous.type].infix;
        infix(compiler);
    }
}

void compile_grouping(Compiler* compiler) {
    compile_expression(compiler);
    parser_consume(&compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

void compile_unary(Compiler* compiler) {
    TokenType operator = compiler->parser.previous.type;

    compile_expression_precedence(compiler, PREC_ASSIGNMENT);

    switch(operator) {
        case TOKEN_MINUS: {
            emit_bytes(compiler, OP_NEG, compiler->regIndex - 1, compiler->regIndex - 1);
            break;
        }
    }
}

void compile_binary(Compiler* compiler) {
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
        default:
            return;
    }

    register_free(compiler);
    emit_bytes(compiler, compiler->regIndex - 1, compiler->regIndex - 1, compiler->regIndex);
}

void emit_byte(Compiler* compiler, uint8_t byte) {
    chunk_write(&compiler->chunk, byte, compiler->parser.previous.index);
}

void emit_bytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3) {
    emit_byte(compiler, byte1);
    emit_byte(compiler, byte2);
    emit_byte(compiler, byte3);
}

void emit_word(Compiler* compiler, uint16_t word) {
    emit_byte(compiler, ((uint8_t*) &word)[0]);
    emit_byte(compiler, ((uint8_t*) &word)[1]);
}

void emit_constant(Compiler* compiler, Value value) {
    uint16_t index = create_constant(compiler, value);

    if(index < UINT8_MAX)
        emit_bytes(compiler, OP_CNST, compiler->regIndex - 1, index);
    else {
        emit_byte(compiler, OP_CNSTW);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_word(compiler, index);
    }
}

void emit_return(Compiler* compiler) {
    emit_byte(compiler, OP_RET);
}

uint8_t create_constant(Compiler* compiler, Value value) {
    size_t index = chunk_write_constant(&compiler->chunk, value);

    if(index > UINT16_MAX) {
        parser_error_at_previous(&compiler->parser, "Constant limit exceeded (65535)");
        return 0;
    }

    return (uint8_t) index;
}

bool register_reserve(Compiler* compiler) {
    if(compiler->regIndex == 249) {
        parser_error_at_current(&compiler->parser, "Register limit exceeded (250)");
        return false;
    }
    else {
        ++compiler->regIndex;
        return true;
    }
}

void register_free(Compiler* compiler) {
    --compiler->regIndex;
}

void finalize_compilation(Compiler* compiler) {
    emit_return(compiler);
}