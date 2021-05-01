#include "parser.h"

#include "../lib/mem_index.h"
#include "../io/log.h"

void risa_parser_init(RisaParser* parser) {
    risa_io_init(&parser->io);
    parser->error = false;
    parser->panic = false;
}

void risa_parser_advance(RisaParser* parser) {
    parser->previous = parser->current;

    while(1) {
        parser->current = risa_lexer_next(&parser->lexer);

        if(parser->current.type != RISA_TOKEN_ERROR)
            break;

        risa_parser_error_at_current(parser, parser->current.start);
    }
}

void risa_parser_consume(RisaParser* parser, RisaTokenType type, const char* err) {
    if(parser->current.type == type) {
        risa_parser_advance(parser);
        return;
    }

    risa_parser_error_at_current(parser, err);
}

void risa_parser_sync(RisaParser* parser) {
    parser->panic = false;

    while(parser->current.type != RISA_TOKEN_EOF) {
        if(parser->previous.type == RISA_TOKEN_SEMICOLON)
            return;

        switch(parser->current.type) {
            case RISA_TOKEN_FUNCTION:
            case RISA_TOKEN_FOR:
            case RISA_TOKEN_IF:
            case RISA_TOKEN_WHILE:
            case RISA_TOKEN_RETURN:
                return;
            default:  ;
        }

        risa_parser_advance(parser);
    }
}

void risa_parser_error_at(RisaParser* parser, RisaToken token, const char* msg) {
    if(parser->panic)
        return;
    parser->panic = true;

    if(token.type == RISA_TOKEN_EOF)
        RISA_ERROR(parser->io, "at EOF: %s\n", msg);
    else if(token.type != RISA_TOKEN_ERROR) {
        size_t ln;
        size_t col;

        risa_lib_mem_lncol(parser->lexer.source, token.index, &ln, &col);
        RISA_ERROR(parser->io, "at %zu:%zu in script: %s\n", ln, col, msg);
    } else {
        size_t ln;
        size_t col;

        risa_lib_mem_lncol(parser->lexer.source, token.index, &ln, &col);
        RISA_ERROR(parser->io, "at %zu:%zu in script: Invalid token\n", ln, col);
    }

    parser->error = true;
}

void risa_parser_error_at_current(RisaParser* parser, const char* msg) {
    risa_parser_error_at(parser, parser->current, msg);
}

void risa_parser_error_at_previous(RisaParser* parser, const char* msg) {
    risa_parser_error_at(parser, parser->previous, msg);
}