#ifndef RISA_COMPILER_H
#define RISA_COMPILER_H

#include "parser.h"

#include "../chunk/chunk.h"
#include "../lexer/lexer.h"
#include "../data/map.h"
#include "../common/def.h"
#include "../value/dense.h"

typedef enum {
    REG_CONSTANT,
    REG_LOCAL,
    REG_UPVAL,
    REG_GLOBAL,
    REG_TEMP,
    REG_EMPTY
} RegType;

typedef struct {
    RegType type;
    Token token;
} RegInfo;

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
    struct Compiler* super;
    DenseFunction* function;

    Parser* parser;
    Map strings;

    RegInfo regs[250];
    uint8_t regIndex;

    struct {
        uint8_t reg;
        bool isNew;
        bool isConst;
        bool isLvalue;
        bool isPostIncrement;
        bool isEqualOp;
        bool canOverwrite;

        struct {
            enum LValType {
                LVAL_LOCAL,       // Local.
                LVAL_GLOBAL,      // Global.
                LVAL_UPVAL,       // Upvalue.
                LVAL_LOCAL_PROP,  // Property of local.
                LVAL_GLOBAL_PROP, // Property of global.
                LVAL_UPVAL_PROP   // Property of upvalue.
            } type;

            uint16_t global;    // The constant string that identifies the global.
            uint8_t globalReg;  // In which register the global temporarily resides.
            uint8_t propOrigin; // The register in which the property holder resides.
            uint8_t upval;

            struct {
                union {
                    uint8_t reg;   // The property index as a register.
                    uint16_t cnst; // The property index as a constant.
                } as;

                bool isConst;      // Whether or not the property index is constant.
            } propIndex;
        } lvalMeta;
    } last;

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
    PREC_UNARY,       // ! - ~ ++pre
    PREC_CALL,        // . () [] post++
    PREC_PRIMARY
} Precedence;

typedef void (*RuleHandler)(Compiler*, bool);

typedef struct {
    RuleHandler prefix;
    RuleHandler inpostfix;

    Precedence precedence;
} Rule;

void compiler_init(Compiler* compiler);
void compiler_delete(Compiler* compiler);

CompilerStatus compiler_compile(Compiler* compiler, const char* str);

#endif
