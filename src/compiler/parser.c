#include "parser.h"

#include "../lib/mem_index.h"
#include "../common/logging.h"

void parser_init(Parser* parser) {
    parser->error = false;
    parser->panic = false;
}

void parser_advance(Parser* parser) {
    parser->previous = parser->current;

    while(1) {
        parser->current = lexer_next(&parser->lexer);

        if(parser->current.type != TOKEN_ERROR)
            break;

        parser_error_at_current(parser, parser->current.start);
    }
}

void parser_consume(Parser* parser, TokenType type, const char* err) {
    if(parser->current.type == type) {
        parser_advance(parser);
        return;
    }

    parser_error_at_current(parser, err);
}

void parser_sync(Parser* parser) {
    parser->panic = false;

    while(parser->current.type != TOKEN_EOF) {
        if(parser->previous.type == TOKEN_SEMICOLON)
            return;

        switch(parser->current.type) {
            case TOKEN_FUNCTION:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default:  ;
        }

        parser_advance(parser);
    }
}

void parser_error_at(Parser* parser, Token token, const char* msg) {
    if(parser->panic)
        return;
    parser->panic = true;

    if(token.type == TOKEN_EOF)
        ERROR("at EOF: %s\n", msg);
    else if(token.type != TOKEN_ERROR) {
        size_t ln;
        size_t col;

        mem_lncol(parser->lexer.source, token.index, &ln, &col);
        ERROR("at %zu:%zu in script: %s\n", ln, col, msg);
    }

    parser->error = true;
}

void parser_error_at_current(Parser* parser, const char* msg) {
    parser_error_at(parser, parser->current, msg);
}

void parser_error_at_previous(Parser* parser, const char* msg) {
    parser_error_at(parser, parser->previous, msg);
}