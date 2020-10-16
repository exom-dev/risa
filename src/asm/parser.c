#include "parser.h"

#include "../lib/mem_index.h"
#include "../common/logging.h"

void asm_parser_init(AsmParser* parser) {
    parser->error = false;
    parser->panic = false;
}

void asm_parser_advance(AsmParser* parser) {
    parser->previous = parser->current;

    while(1) {
        parser->current = asm_lexer_next(&parser->lexer);

        if(parser->current.type != ASM_TOKEN_ERROR)
            break;

        asm_parser_error_at_current(parser, parser->current.start);
    }
}

void asm_parser_consume(AsmParser* parser, AsmTokenType type, const char* err) {
    if(parser->current.type == type) {
        asm_parser_advance(parser);
        return;
    }

    asm_parser_error_at_current(parser, err);
}

void asm_parser_sync(AsmParser* parser) {
    parser->panic = false;

    while(parser->current.type != ASM_TOKEN_EOF) {
        switch(parser->current.type) {
            case ASM_TOKEN_DOT:
            case ASM_TOKEN_STRING:
            case ASM_TOKEN_TRUE:
            case ASM_TOKEN_FALSE:
            case ASM_TOKEN_BYTE:
            case ASM_TOKEN_INT:
            case ASM_TOKEN_FLOAT:
            case ASM_TOKEN_REGISTER:
            case ASM_TOKEN_CONSTANT:
                asm_parser_advance(parser);
                continue;
            default: return;
        }
    }
}

void asm_parser_error_at(AsmParser* parser, AsmToken token, const char* msg) {
    if(parser->panic)
        return;
    parser->panic = true;

    if(token.type == ASM_TOKEN_EOF)
        ERROR("at EOF: %s\n", msg);
    else if(token.type != ASM_TOKEN_ERROR) {
        size_t ln;
        size_t col;

        mem_lncol(parser->lexer.source, token.index, &ln, &col);
        ERROR("at %zu:%zu in script: %s\n", ln, col, msg);
    }

    parser->error = true;
}

void asm_parser_error_at_current(AsmParser* parser, const char* msg) {
    asm_parser_error_at(parser, parser->current, msg);
}

void asm_parser_error_at_previous(AsmParser* parser, const char* msg) {
    asm_parser_error_at(parser, parser->previous, msg);
}