#ifndef RISA_COMPILER_H
#define RISA_COMPILER_H

#include "parser.h"

#include "../chunk/chunk.h"
#include "../lexer/lexer.h"
#include "../data/map.h"
#include "../common/def.h"

typedef struct {
    Token identifier;
    int32_t depth;
    uint8_t reg;

    bool captured;
} Local;

typedef struct {
    uint8_t index;
    bool local;
} Upvalue;

typedef struct {
    uint32_t index;
    uint8_t depth;
    bool isBreak;
} Leap;

typedef struct Compiler {
    struct Compiler* enclosing;
    DenseFunction* function;

    Parser* parser;
    Map strings;

    uint8_t regIndex;

    Local locals[250];
    Upvalue upvalues[250];
    Leap leaps[250];

    uint8_t localCount;
    uint8_t upvalueCount;
    uint8_t loopCount;
    uint8_t leapCount;

    int32_t scopeDepth;
} Compiler;

typedef enum {
    COMPILER_OK,
    COMPILER_ERROR
} CompilerStatus;

typedef enum {
    PREC_NONE,
    PREC_COMMA,       // ,
    PREC_ASSIGNMENT,  // =
    PREC_TERNARY,     // ?:
    PREC_OR,          // ||
    PREC_AND,         // &&
    PREC_BITWISE_OR,  // |
    PREC_BITWISE_XOR, // ^
    PREC_BITWISE_AND, // &
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! - ~
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*RuleHandler)(Compiler*, bool);

typedef struct {
    RuleHandler prefix;
    RuleHandler infix;

    Precedence precedence;
} Rule;

void compiler_init(Compiler* compiler);
void compiler_delete(Compiler* compiler);

CompilerStatus compiler_compile(Compiler* compiler, const char* str);

#endif
