#include "compiler.h"
#include "parser.h"

#include "../value/dense.h"
#include "../chunk/bytecode.h"
#include "../common/logging.h"
#include "../lexer/lexer.h"

#include <string.h>
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
static void compile_function_declaration(Compiler* compiler);
static void compile_function(Compiler* compiler);
static void compile_statement(Compiler* compiler);
static void compile_if_statement(Compiler* compiler);
static void compile_while_statement(Compiler* compiler);
static void compile_for_statement(Compiler* compiler);
static void compile_return_statement(Compiler* compiler);
static void compile_continue_statement(Compiler* compiler);
static void compile_break_statement(Compiler* compiler);
static void compile_block(Compiler* compiler);
static void compile_expression_statement(Compiler* compiler);
static void compile_expression(Compiler* compiler);
static void compile_expression_precedence(Compiler* compiler, Precedence precedence);
static void compile_return_expression(Compiler* compiler);
static void compile_call(Compiler* compiler, bool);
static void compile_grouping_or_lambda(Compiler* compiler, bool);
static void compile_lambda(Compiler* compiler);
static void compile_unary(Compiler* compiler, bool);
static void compile_binary(Compiler* compiler, bool);
static void compile_ternary(Compiler* compiler, bool);
static void compile_and(Compiler* compiler, bool);
static void compile_or(Compiler* compiler, bool);
static void compile_comma(Compiler* compiler, bool);

static uint8_t compile_arguments(Compiler* compiler);

static void scope_begin(Compiler* compiler);
static void scope_end(Compiler* compiler);

static void    local_add(Compiler* compiler, Token identifier);
static uint8_t local_resolve(Compiler* compiler, Token* identifier);

static uint8_t upvalue_add(Compiler* compiler, uint8_t index, bool local);
static uint8_t upvalue_resolve(Compiler* compiler, Token* identifier);

static void     emit_byte(Compiler* compiler, uint8_t byte);
static void     emit_bytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, uint8_t byte3);
static void     emit_word(Compiler* compiler, uint16_t word);
static void     emit_constant(Compiler* compiler, Value value);
static void     emit_return(Compiler* compiler);
static void     emit_jump(Compiler* compiler, uint32_t index);
static void     emit_backwards_jump(Compiler* compiler, uint32_t to);
static void     emit_backwards_jump_from(Compiler* compiler, uint32_t from, uint32_t to);
static uint32_t emit_blank(Compiler* compiler);

static uint16_t create_constant(Compiler* compiler, Value value);
static uint16_t create_identifier_constant(Compiler* compiler);
static uint16_t create_string_constant(Compiler* compiler, const char* start, uint32_t length);
static uint16_t declare_variable(Compiler* compiler);

static bool register_reserve(Compiler* compiler);
static void register_free(Compiler* compiler);

static void finalize_compilation(Compiler* compiler);

Rule EXPRESSION_RULES[] = {
        {compile_grouping_or_lambda, compile_call,  PREC_CALL },// TOKEN_LEFT_PAREN
        { NULL,     NULL,                           PREC_NONE },// TOKEN_RIGHT_PAREN
        { NULL,     NULL,                           PREC_NONE },// TOKEN_LEFT_BRACKET
        { NULL,     NULL,                           PREC_NONE },// TOKEN_RIGHT_BRACKET
        { NULL,     NULL,                           PREC_NONE },// TOKEN_LEFT_BRACE
        { NULL,     NULL,                           PREC_NONE },// TOKEN_RIGHT_BRACE
        { NULL,                      compile_comma, PREC_COMMA },// TOKEN_COMMA
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
        { NULL,     compile_ternary,    PREC_TERNARY },   // TOKEN_QUESTION
        { compile_unary,     NULL,    PREC_NONE },       // TOKEN_BANG
        { NULL,     compile_binary,    PREC_EQUALITY },  // TOKEN_BANG_EQUAL
        { NULL,     NULL,    PREC_NONE },                // TOKEN_EQUAL
        { NULL,     compile_binary,    PREC_EQUALITY },  // TOKEN_EQUAL_EQUAL
        { NULL,     NULL,    PREC_NONE },                // TOKEN_EQUAL_GREATER
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_GREATER
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_GREATER_EQUAL
        { NULL,     compile_binary,    PREC_SHIFT },     // TOKEN_GREATER_GREATER
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_LESS
        { NULL,     compile_binary,    PREC_COMPARISON },// TOKEN_LESS_EQUAL
        { NULL,     compile_binary,    PREC_SHIFT },     // TOKEN_LESS_LESS
        { NULL,     compile_binary,    PREC_BITWISE_AND },// TOKEN_AMPERSAND
        { NULL,     compile_and,    PREC_AND },          // TOKEN_AMPERSAND_AMPERSAND
        { NULL,     compile_binary,    PREC_BITWISE_OR },// TOKEN_PIPE
        { NULL,     compile_or,    PREC_OR },          // TOKEN_PIPE_PIPE
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
        { NULL,     NULL,    PREC_NONE },                // TOKEN_CONTINUE
        { NULL,     NULL,    PREC_NONE },                // TOKEN_BREAK
        { NULL,     NULL,    PREC_NONE },                // TOKEN_ERROR
        { NULL,     NULL,    PREC_NONE },                // TOKEN_EOF
};

void compiler_init(Compiler* compiler) {
    compiler->enclosing = NULL;
    compiler->function = dense_function_create();

    chunk_init(&compiler->function->chunk);
    map_init(&compiler->strings);

    compiler->regIndex = 0;

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
    }

    finalize_compilation(compiler);

    return compiler->parser->error ? COMPILER_ERROR : COMPILER_OK;
}

static void compile_byte(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

    if(errno == ERANGE || num > 256) {
        parser_error_at_previous(compiler->parser, "Number is too large for type 'byte'");
        return;
    }

    emit_constant(compiler, BYTE_VALUE(num));
}

static void compile_int(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    int64_t num = strtol(compiler->parser->previous.start, NULL, 10);

    if(errno == ERANGE) {
        parser_error_at_previous(compiler->parser, "Number is too large for type 'int'");
        return;
    }

    emit_constant(compiler, INT_VALUE(num));
}

static void compile_float(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    double num = strtod(compiler->parser->previous.start, NULL);

    if(errno == ERANGE) {
        parser_error_at_previous(compiler->parser, "Number is too small or too large for type 'float'");
        return;
    }

    emit_constant(compiler, FLOAT_VALUE(num));
}

static void compile_string(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    const char* start = compiler->parser->previous.start + 1;
    uint32_t length = compiler->parser->previous.size - 2;

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

    Compiler* super = compiler->enclosing == NULL ? compiler : compiler->enclosing;

    while(super->enclosing != NULL)
        super = super->enclosing;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    MEM_FREE(str);

    emit_constant(compiler, DENSE_VALUE(interned));
}

static void compile_literal(Compiler* compiler, bool allowAssignment) {
    if(!register_reserve(compiler))
        return;

    switch(compiler->parser->previous.type) {
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
        compile_expression(compiler);

        emit_byte(compiler, set);
        emit_byte(compiler, index);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);

        //register_free(compiler);
    } else {
        if(!register_reserve(compiler))
            return;

        emit_byte(compiler, get);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, index);
        emit_byte(compiler, 0);
    }
}

static void compile_declaration(Compiler* compiler) {
    if(compiler->parser->current.type == TOKEN_VAR) {
        parser_advance(compiler->parser);
        compile_variable_declaration(compiler);
    } else if(compiler->parser->current.type == TOKEN_FUNCTION) {
        parser_advance(compiler->parser);
        compile_function_declaration(compiler);
    } else compile_statement(compiler);

    if(compiler->parser->panic)
        parser_sync(compiler->parser);
}

static void compile_variable_declaration(Compiler* compiler) {
    uint16_t index = declare_variable(compiler);

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
    }

    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration");

    if(compiler->scopeDepth > 0) {
        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
        return;
    }

    register_free(compiler);

    emit_byte(compiler, OP_DGLOB);
    emit_byte(compiler, compiler->regIndex);
    emit_byte(compiler, index);
    emit_byte(compiler, 0);
}

static void compile_function_declaration(Compiler* compiler) {
    uint16_t index = declare_variable(compiler);

    if(compiler->scopeDepth > 0)
        compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;

    compile_function(compiler);

    if(compiler->scopeDepth > 0)
        return;

    register_free(compiler);

    emit_byte(compiler, OP_DGLOB);
    emit_byte(compiler, compiler->regIndex);
    emit_byte(compiler, index);
    emit_byte(compiler, 0);
}

static void compile_function(Compiler* compiler) {
    Compiler subcompiler;
    compiler_init(&subcompiler);

    const char* start = compiler->parser->previous.start;
    uint32_t length = compiler->parser->previous.size;
    uint32_t hash = map_hash(start, length);

    Compiler* super = compiler->enclosing == NULL ? compiler : compiler->enclosing;

    while(super->enclosing != NULL)
        super = super->enclosing;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.enclosing = compiler;
    subcompiler.parser = compiler->parser;

    scope_begin(&subcompiler);

    parser_consume(subcompiler.parser, TOKEN_LEFT_PAREN, "Expected '(' after function name");

    if(subcompiler.parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            ++subcompiler.function->arity;

            if (subcompiler.function->arity > 250) {
                parser_error_at_current(subcompiler.parser, "Parameter limit exceeded (250)");
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

    if(!register_reserve(compiler))
        return;

    if(subcompiler.upvalueCount == 0)
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));
    else {
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

    compiler_delete(&subcompiler);
}

static void compile_statement(Compiler* compiler) {
    if (compiler->parser->current.type == TOKEN_IF) {
        parser_advance(compiler->parser);
        compile_if_statement(compiler);
    } else if (compiler->parser->current.type == TOKEN_WHILE) {
        parser_advance(compiler->parser);
        compile_while_statement(compiler);
    } else if (compiler->parser->current.type == TOKEN_FOR) {
        parser_advance(compiler->parser);
        compile_for_statement(compiler);
    } else if (compiler->parser->current.type == TOKEN_RETURN) {
        parser_advance(compiler->parser);
        compile_return_statement(compiler);
    } else if (compiler->parser->current.type == TOKEN_CONTINUE) {
        parser_advance(compiler->parser);
        compile_continue_statement(compiler);
    } else if (compiler->parser->current.type == TOKEN_BREAK) {
        parser_advance(compiler->parser);
        compile_break_statement(compiler);
    } else if(compiler->parser->current.type == TOKEN_LEFT_BRACE) {
        parser_advance(compiler->parser);

        scope_begin(compiler);
        compile_block(compiler);
        scope_end(compiler);
    } else {
        compile_expression_statement(compiler);
    }
}

static void compile_if_statement(Compiler* compiler) {
    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->regIndex - 1);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    register_free(compiler);

    uint32_t ifEnd = emit_blank(compiler);

    compile_statement(compiler);

    uint32_t elseEnd = emit_blank(compiler);

    emit_jump(compiler, ifEnd);

    if(compiler->parser->current.type == TOKEN_ELSE) {
        parser_advance(compiler->parser);
        compile_statement(compiler);
    }

    emit_jump(compiler, elseEnd);
}

static void compile_while_statement(Compiler* compiler) {
    uint32_t start = compiler->function->chunk.size;

    parser_consume(compiler->parser, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    compile_expression(compiler);
    parser_consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition");

    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->regIndex - 1);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

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
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

        register_free(compiler);

        exitIndex = emit_blank(compiler);
        infinite = false;
    }

    if(compiler->parser->current.type != TOKEN_RIGHT_PAREN) {
        uint32_t bodyJump = emit_blank(compiler);
        uint32_t post = compiler->function->chunk.size;

        compile_expression(compiler);
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
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

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
        compile_expression(compiler);

        emit_byte(compiler, OP_RET);
        emit_byte(compiler, compiler->regIndex - 1);
        emit_byte(compiler, 0);
        emit_byte(compiler, 0);

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

static void compile_expression_statement(Compiler* compiler) {
    compile_expression(compiler);

    parser_consume(compiler->parser, TOKEN_SEMICOLON, "Expected ';' after expression");

    register_free(compiler);
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

        RuleHandler infix = EXPRESSION_RULES[compiler->parser->previous.type].infix;
        infix(compiler, allowAssignment);
    }

    if(allowAssignment && (compiler->parser->current.type == TOKEN_EQUAL)) {
        parser_error_at_previous(compiler->parser, "Invalid assignment target");
    }
}

static void compile_call(Compiler* compiler, bool allowAssignment) {
    uint8_t functionReg = compiler->regIndex - 1;
    uint8_t argc = compile_arguments(compiler);

    emit_byte(compiler, OP_CALL);
    emit_byte(compiler, functionReg);
    emit_byte(compiler, argc);
    emit_byte(compiler, 0);

    while(argc > 0) {
        register_free(compiler);
        --argc;
    }
}

static uint8_t compile_arguments(Compiler* compiler) {
    uint8_t argc = 0;

    if(compiler->parser->current.type != TOKEN_RIGHT_PAREN) {
        do {
            compile_expression_precedence(compiler, PREC_COMMA + 1);

            if(argc == 255)
                parser_error_at_previous(compiler->parser, "Argument limit exceeded (255)");

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

    Compiler* super = compiler->enclosing == NULL ? compiler : compiler->enclosing;

    while(super->enclosing != NULL)
        super = super->enclosing;

    DenseString* interned = map_find(&super->strings, start, length, hash);

    if(interned == NULL) {
        interned = dense_string_from(start, length);
        map_set(&super->strings, interned, NULL_VALUE);
    }

    subcompiler.function->name = interned;
    subcompiler.enclosing = compiler;
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

            ++subcompiler.regIndex;
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

    if(subcompiler.upvalueCount == 0)
        emit_constant(compiler, DENSE_VALUE(subcompiler.function));
    else {
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

    compiler_delete(&subcompiler);
}

static void compile_unary(Compiler* compiler, bool allowAssignment) {
    TokenType operator = compiler->parser->previous.type;

    compile_expression_precedence(compiler, PREC_UNARY);

    switch(operator) {
        case TOKEN_BANG:
            emit_bytes(compiler, OP_NOT, compiler->regIndex - 1, compiler->regIndex - 1);
            emit_byte(compiler, 0);
            break;
        case TOKEN_TILDE:
            emit_bytes(compiler, OP_BNOT, compiler->regIndex - 1, compiler->regIndex - 1);
            emit_byte(compiler, 0);
            break;
        case TOKEN_MINUS:
            emit_bytes(compiler, OP_NEG, compiler->regIndex - 1, compiler->regIndex - 1);
            emit_byte(compiler, 0);
            break;
        default:
            return;
    }
}

static void compile_binary(Compiler* compiler, bool allowAssignment) {
    TokenType operatorType = compiler->parser->previous.type;

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

static void compile_and(Compiler* compiler, bool allowAssignment) {
    emit_byte(compiler, OP_TEST);
    emit_byte(compiler, compiler->regIndex - 1);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    register_free(compiler);

    uint16_t index = emit_blank(compiler);

    compile_expression_precedence(compiler, PREC_AND);

    emit_jump(compiler, index);
}

static void compile_or(Compiler* compiler, bool allowAssignment) {
    emit_byte(compiler, OP_NTEST);
    emit_byte(compiler, compiler->regIndex - 1);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);

    register_free(compiler);

    uint16_t index = emit_blank(compiler);

    compile_expression_precedence(compiler, PREC_OR);

    emit_jump(compiler, index);
}

static void compile_comma(Compiler* compiler, bool allowAssignment) {
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
    if(compiler->localCount > 250) {
        parser_error_at_previous(compiler->parser, "Local variable limit exceeded (250)");
        return;
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->identifier = identifier;
    local->depth = -1;
    local->reg = compiler->regIndex;
    local->captured = false;
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
    if(compiler->enclosing == NULL)
        return 251;

    uint8_t local = local_resolve(compiler->enclosing, identifier);

    if(local != 251) {
        compiler->enclosing->locals[local].captured = true;
        return upvalue_add(compiler, local, true);
    }

    uint8_t upvalue = upvalue_resolve(compiler->enclosing, identifier);

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
    emit_byte(compiler, OP_RET);
    emit_byte(compiler, 251);
    emit_byte(compiler, 0);
    emit_byte(compiler, 0);
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

    Compiler* super = compiler->enclosing == NULL ? compiler : compiler->enclosing;

    while(super->enclosing != NULL)
        super = super->enclosing;

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