#include "parser.h"

#include "../lib/mem_index.h"
#include "../io/log.h"

void risa_asm_parser_init(RisaAsmParser* parser) {
    risa_io_init(&parser->io);
    parser->error = false;
    parser->panic = false;
}

void risa_asm_parser_advance(RisaAsmParser* parser) {
    parser->previous = parser->current;

    while(1) {
        parser->current = risa_asm_lexer_next(&parser->lexer);

        if(parser->current.type != RISA_ASM_TOKEN_ERROR)
            break;

        risa_asm_parser_error_at_current(parser, parser->current.start);
    }
}

void risa_asm_parser_consume(RisaAsmParser* parser, RisaAsmTokenType type, const char* err) {
    if(parser->current.type == type) {
        risa_asm_parser_advance(parser);
        return;
    }

    risa_asm_parser_error_at_current(parser, err);
}

void risa_asm_parser_sync(RisaAsmParser* parser) {
    parser->panic = false;

    while(parser->current.type != RISA_ASM_TOKEN_EOF) {
        switch(parser->current.type) {
            case RISA_ASM_TOKEN_DOT:
            case RISA_ASM_TOKEN_STRING:
            case RISA_ASM_TOKEN_TRUE:
            case RISA_ASM_TOKEN_FALSE:
            case RISA_ASM_TOKEN_BYTE:
            case RISA_ASM_TOKEN_INT:
            case RISA_ASM_TOKEN_FLOAT:
            case RISA_ASM_TOKEN_REGISTER:
            case RISA_ASM_TOKEN_CONSTANT:
                risa_asm_parser_advance(parser);
                continue;
            default: return;
        }
    }
}

void risa_asm_parser_error_at(RisaAsmParser* parser, RisaAsmToken token, const char* msg) {
    if(parser->panic)
        return;
    parser->panic = true;

    if(token.type == RISA_ASM_TOKEN_EOF)
        RISA_ERROR(parser->io, "at EOF: %s\n", msg);
    else if(token.type != RISA_ASM_TOKEN_ERROR) {
        size_t ln;
        size_t col;

        risa_lib_mem_lncol(parser->lexer.source, token.index, &ln, &col);
        RISA_ERROR(parser->io, "at %zu:%zu in script: %s\n", ln, col, msg);
    }

    parser->error = true;
}

void risa_asm_parser_error_at_current(RisaAsmParser* parser, const char* msg) {
    risa_asm_parser_error_at(parser, parser->current, msg);
}

void risa_asm_parser_error_at_previous(RisaAsmParser* parser, const char* msg) {
    risa_asm_parser_error_at(parser, parser->previous, msg);
}