#ifndef RISA_PARSER_H_GUARD
#define RISA_PARSER_H_GUARD

#include "../io/io.h"
#include "lexer.h"
#include "../def/types.h"

typedef struct {
    RisaIO io;
    RisaLexer lexer;

    RisaToken current;
    RisaToken previous;

    bool error;
    bool panic;
} RisaParser;

void risa_parser_init              (RisaParser* parser);
void risa_parser_advance           (RisaParser* parser);
void risa_parser_consume           (RisaParser* parser, RisaTokenType type, const char* err);

void risa_parser_sync              (RisaParser* parser);

void risa_parser_error_at          (RisaParser* parser, RisaToken token, const char* msg);
void risa_parser_error_at_current  (RisaParser* parser, const char* msg);
void risa_parser_error_at_previous (RisaParser* parser, const char* msg);

#endif
