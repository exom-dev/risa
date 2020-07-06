#include "compiler.h"

#include "../common/logging.h"
#include "../lexer/lexer.h"

CompilerStatus compiler_compile(const char* str, Chunk* chunk) {
    Lexer lexer;
    lexer_init(&lexer);
    lexer_source(&lexer, str);

    while(1) {
        Token token = lexer_next(&lexer);

        PRINT("%d\n", token.type + 1);

        if(token.type == TOKEN_ERROR) {
            lexer_delete(&lexer);
            return COMPILER_ERROR;
        }

        if(token.type == TOKEN_EOF)
            break;
    }

    return COMPILER_OK;
}