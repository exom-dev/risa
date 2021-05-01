#ifndef RISA_ASM_PARSER_H_GUARD
#define RISA_ASM_PARSER_H_GUARD

#include "lexer.h"
#include "../api.h"
#include "../io/io.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    RisaAsmLexer lexer;

    RisaAsmToken current;
    RisaAsmToken previous;

    bool error;
    bool panic;
} RisaAsmParser;

RISA_API void risa_asm_parser_init              (RisaAsmParser* parser);
RISA_API void risa_asm_parser_advance           (RisaAsmParser* parser);
RISA_API void risa_asm_parser_consume           (RisaAsmParser* parser, RisaAsmTokenType type, const char* err);

RISA_API void risa_asm_parser_sync              (RisaAsmParser* parser);
RISA_API void risa_asm_parser_error_at          (RisaAsmParser* parser, RisaAsmToken token, const char* msg);
RISA_API void risa_asm_parser_error_at_previous (RisaAsmParser* parser, const char* msg);
RISA_API void risa_asm_parser_error_at_current  (RisaAsmParser* parser, const char* msg);

#endif
