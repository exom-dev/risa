#include "compiler.h"
#include "parser.h"

#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../lexer/lexer.h"

#include <stdlib.h>

#define WORD_RIGHT_MASK 0x

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

void compiler_init(Compiler* compiler) {
    chunk_init(&compiler->chunk);
    parser_init(&compiler->parser);

    compiler->regIndex = 0;
}

void compiler_delete(Compiler* compiler) {
    emit_return(compiler);
}

CompilerStatus compiler_compile(Compiler* compiler, const char* str) {
    parser_init(&compiler->parser);
    lexer_init(&compiler->parser.lexer);
    lexer_source(&compiler->parser.lexer, str);

    parser_advance(&compiler->parser);
    compile_expression(compiler);

    parser_consume(&compiler->parser, TOKEN_EOF, "Expected end of expression");

    return !compiler->parser.error;
}

void compile_number(Compiler* compiler) {
    double num = strtod(compiler->parser.previous.start, NULL);
    emit_constant(compiler, num);
}

void compile_expression(Compiler* compiler) {
    compile_expression_precedence(PREC_ASSIGNMENT);
}

void compile_expression_precedence(Compiler* compiler, Precedence precedence) {

}

void compile_grouping(Compiler* compiler) {
    compile_expression(compiler);
    parser_consume(&compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

void compile_unary(Compiler* compiler) {
    TokenType operator = compiler->parser.previous.type;

    uint8_t reg = compiler->regIndex;
    if(!register_reserve(compiler))
        return;

    register_free(compiler);
    compile_expression_precedence(compiler, PREC_ASSIGNMENT);

    switch(operator) {
        case TOKEN_MINUS: {
            emit_bytes(compiler, OP_NEG, reg, reg);
            break;
        }
    }
}

void compile_binary(Compiler* compiler) {
    TokenType operator = compiler->parser.previous.type;
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
    emit_bytes(compiler, ((uint8_t*) &word)[0],  ((uint8_t*) &word)[1]);
}

void emit_constant(Compiler* compiler, Value value) {
    uint16_t index = create_constant(compiler, value);

    uint8_t reg = compiler->regIndex;
    if(!register_reserve(compiler))
        return;

    if(index < UINT8_MAX)
        emit_bytes(compiler, OP_CNST, reg, index);
    else {
        emit_byte(compiler, OP_CNSTW);
        emit_byte(compiler, reg);
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
    if(compiler->regIndex == 249)
        parser_error_at_current(&compiler->parser, "Register limit exceeded (250)");
    else ++compiler->regIndex;
}

void register_free(Compiler* compiler) {
    --compiler->regIndex;
}