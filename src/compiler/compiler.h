#ifndef RISA_COMPILER_H
#define RISA_COMPILER_H

#include "lexer.h"
#include "parser.h"

#include "../api.h"
#include "../io/io.h"
#include "../cluster/cluster.h"
#include "../data/map.h"
#include "../def/def.h"
#include "../value/dense.h"
#include "../options/options.h"

typedef enum {
    RISA_REG_CONSTANT,
    RISA_REG_LOCAL,
    RISA_REG_UPVAL,
    RISA_REG_GLOBAL,
    RISA_REG_TEMP,
    RISA_REG_EMPTY
} RisaRegType;

typedef struct {
    RisaRegType type;
    RisaToken token;
} RisaRegInfo;

typedef struct {
    RisaToken identifier;
    int32_t depth;
    uint8_t reg;

    bool captured;
} RisaLocalInfo;

typedef struct {
    uint8_t index;
    bool local;
} RisaUpvalueInfo;

typedef struct {
    uint32_t index;
    uint8_t depth;
    bool isBreak;
} RisaLeapInfo;

typedef struct RisaCompiler {
    RisaIO io;

    struct RisaCompiler* super;
    RisaDenseFunction* function;

    RisaParser* parser;
    RisaMap strings;

    RisaRegInfo regs[250];
    uint8_t regIndex;

    RisaOptions options;

    struct {
        uint8_t reg;          // In which register the last value resides.
        bool isNew;           // Whether or not the last value resides in a newly-reserved register.
        bool isConst;         // Whether or not the last value is a constant.
        bool isLvalue;        // Whether or not the last value is a lvalue.
        bool isPostIncrement; // Whether or not the last value is the result of a post increment operation.
        bool isEqualOp;       // Whether or not the last value is the result of an equality operation.
        bool canOverwrite;    // Whether or not the last value register can be overwritten. Used to prevent an object property from overwriting a global value in a temporary reg.
        bool fromBranched;    // Whether or not the last value comes from one of multiple branches (e.g. ternary). Required to disable bad CNST optimizations.

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

    RisaLocalInfo locals[250];
    RisaUpvalueInfo upvalues[250];
    RisaLeapInfo leaps[250];

    uint8_t localCount;
    uint8_t upvalueCount;
    uint8_t loopCount;
    uint8_t leapCount;

    int32_t scopeDepth;
} RisaCompiler;

typedef enum {
    RISA_COMPILER_STATUS_OK,
    RISA_COMPILER_STATUS_ERROR
} RisaCompilerStatus;

typedef enum {
    RISA_PREC_NONE,
    RISA_PREC_COMMA,       // ,
    RISA_PREC_ASSIGNMENT,  // =
    RISA_PREC_TERNARY,     // ?:
    RISA_PREC_OR,          // ||
    RISA_PREC_AND,         // &&
    RISA_PREC_BITWISE_OR,  // |
    RISA_PREC_BITWISE_XOR, // ^
    RISA_PREC_BITWISE_AND, // &
    RISA_PREC_EQUALITY,    // == !=
    RISA_PREC_COMPARISON,  // < > <= >=
    RISA_PREC_SHIFT,       // << >>
    RISA_PREC_TERM,        // + -
    RISA_PREC_FACTOR,      // * / %
    RISA_PREC_UNARY,       // ! - ~ ++pre
    RISA_PREC_CALL,        // . () [] post++
    RISA_PREC_PRIMARY
} RisaOperatorPrecedence;

typedef void (*RisaOperatorRuleHandler)(RisaCompiler*, bool);

typedef struct {
    RisaOperatorRuleHandler prefix;
    RisaOperatorRuleHandler inpostfix; // infix or postfix

    RisaOperatorPrecedence precedence;
} RisaOperatorRule;

RISA_API RisaCompiler*      risa_compiler_create        ();
RISA_API void               risa_compiler_init          (RisaCompiler* compiler);
RISA_API RisaIO*            risa_compiler_get_io        (RisaCompiler* compiler);
RISA_API RisaDenseFunction* risa_compiler_get_function  (RisaCompiler* compiler);
RISA_API RisaMap*           risa_compiler_get_strings   (RisaCompiler* compiler);
RISA_API void               risa_compiler_set_repl_mode (RisaCompiler* compiler, bool value);
RISA_API void               risa_compiler_delete        (RisaCompiler* compiler);
RISA_API void               risa_compiler_free          (RisaCompiler* compiler);
RISA_API void               risa_compiler_shallow_free  (RisaCompiler* compiler);

RISA_API RisaCompilerStatus risa_compiler_compile (RisaCompiler* compiler, const char* str);

#endif
