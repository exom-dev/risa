#include "lexer.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (++lexer->index, lexer->current += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0')
#define MATCH(c) \
    ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true))

bool asm_is_digit(char c);
bool asm_is_alpha(char c);

Token asm_next_identifier(Lexer* lexer);
Token asm_next_number(Lexer* lexer);
Token asm_next_string(Lexer* lexer);

void asm_lexer_init(Lexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->index = 0;
}

void asm_lexer_source(Lexer* lexer, const char* src) {
    lexer->source = src;
    lexer->start = src;
    lexer->current = src;
}

void asm_lexer_delete(Lexer* lexer) {
    asm_lexer_init(lexer);
}

Token asm_lexer_next(Lexer* lexer) {
    bool whitespace = true;

    while(!AT_END(0) && whitespace) {
        switch(PEEK(0)) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                ADVANCE(1);
                break;
            case '/':
                if(PEEK(1) == '/') {
                    ADVANCE(2);

                    while(!AT_END(0) && PEEK(0) != '\n')
                        ADVANCE(1);
                } else if(PEEK(1) == '*') {
                    ADVANCE(2);

                    while(!AT_END(0)) {
                        if(PEEK(0) == '*' && !AT_END(1) && PEEK(1) == '/')
                            break;
                        ADVANCE(1);
                    }

                    if(AT_END(0))
                        return asm_lexer_error(lexer, "Expected end of comment block");

                    ADVANCE(2);
                } else whitespace = false;

                break;
            default:
                whitespace = false;
        }
    }

    lexer->start = lexer->current;

    if(AT_END(0))
        return asm_lexer_emit(lexer, TOKEN_EOF);

    char c = NEXT();

    if(asm_is_alpha(c))
        return asm_next_identifier(lexer);
    if(asm_is_digit(c))
        return asm_next_number(lexer);

    switch(c) {
        case '.': return asm_lexer_emit(lexer, TOKEN_DOT);
        case ',': return asm_lexer_emit(lexer, TOKEN_COMMA);
        case '"': return asm_next_string(lexer);
        default: return asm_lexer_error(lexer, "Unexpected character");
    }
}

Token asm_lexer_emit(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.size = (size_t) (lexer->current - lexer->start);
    token.index = lexer->index - token.size;

    return token;
}

Token asm_lexer_error(Lexer* lexer, const char* msg) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = msg;
    token.size = strlen(msg);
    token.index = lexer->index;

    return token;
}

bool asm_is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool asm_is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_');
}

Token asm_next_identifier(Lexer* lexer) {
    #define CLASSIFY(index, length, str, type) \
        (((lexer->current - lexer->start == index + length) && \
        (memcmp(lexer->start + index, str, length) == 0)) ? type : TOKEN_IDENTIFIER)

    while(!AT_END(0) && (asm_is_alpha(PEEK(0)) || asm_is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'a':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'd': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "d", TOKEN_ADD));
                    case 'r': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "r", TOKEN_ARR));
                }
            }
        case 'b':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "nd", TOKEN_BAND));
                    case 'j': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "mpw", TOKEN_BJMPW) == TOKEN_BJMPW ? TOKEN_BJMPW :
                                                           CLASSIFY(2, 2, "mp", TOKEN_BJMP));
                    case 'n': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "ot", TOKEN_BNOT));
                    case 'o': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "r", TOKEN_BOR) == TOKEN_BOR ? TOKEN_BOR :
                                                           CLASSIFY(2, 2, "ol", TOKEN_BOOL_TYPE));
                    case 'x': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "or", TOKEN_BXOR));
                    case 'y': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "te", TOKEN_BYTE_TYPE));
                }
            }
        case 'c':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "ll", TOKEN_CALL));
                    case 'l':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'o': return asm_lexer_emit(lexer, CLASSIFY(3, 2, "ne", TOKEN_CLONE));
                                case 's': return asm_lexer_emit(lexer, CLASSIFY(3, 1, "r", TOKEN_CLSR));
                            }
                        }
                    case 'n': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "stw", TOKEN_CNSTW) == TOKEN_CNSTW ? TOKEN_CNSTW :
                                                           CLASSIFY(2, 2, "st", TOKEN_CNST));
                    case 'o': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "de", TOKEN_CODE));
                    case 'u': return asm_lexer_emit(lexer, CLASSIFY(2, 4, "pval", TOKEN_CUPVAL));
                }
            }
        case 'd':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "ta", TOKEN_DATA));
                    case 'e': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "c", TOKEN_DEC));
                    case 'i': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "v", TOKEN_DIV));
                    case 'g': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "lob", TOKEN_DGLOB));
                }
            }
        case 'e': return asm_lexer_emit(lexer, CLASSIFY(1, 1, "e", TOKEN_EQ));
        case 'f':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "lse", TOKEN_FALSE));
                    case 'l': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "oat", TOKEN_FLOAT_TYPE));
                    case 'u': return asm_lexer_emit(lexer, CLASSIFY(2, 6, "nction", TOKEN_FUNCTION_TYPE));
                }
            }
            return asm_lexer_emit(lexer, CLASSIFY(1, 4, "alse", TOKEN_FALSE));
        case 'g':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "t", TOKEN_GET));
                    case 'g': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "lob", TOKEN_GGLOB));
                    case 't': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "e", TOKEN_GTE) == TOKEN_GTE ? TOKEN_GTE : TOKEN_GT);
                    case 'u': return asm_lexer_emit(lexer, CLASSIFY(2, 4, "pval", TOKEN_GUPVAL));
                }
            }
        case 'i':
            return asm_lexer_emit(lexer, CLASSIFY(1, 1, "nc", TOKEN_INC) == TOKEN_INC ? TOKEN_INC :
                                         CLASSIFY(1, 1, "nt", TOKEN_INT_TYPE));
        case 'j': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "mpw", TOKEN_JMPW) == TOKEN_JMPW ? TOKEN_JMPW :
                                               CLASSIFY(2, 2, "mp", TOKEN_JMP));
        case 'l':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "n", TOKEN_LEN));
                    case 't': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "e", TOKEN_LTE) == TOKEN_LTE ? TOKEN_LTE : TOKEN_LT);
                }
            }
        case 'm':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'o': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "d", TOKEN_MOD) == TOKEN_MOV ? TOKEN_MOV :
                                                           CLASSIFY(2, 1, "d", TOKEN_MOD));
                    case 'u': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "l", TOKEN_MUL));
                }
            }
        case 'n':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "g", TOKEN_NEG) == TOKEN_NEG ? TOKEN_NEG :
                                                           CLASSIFY(2, 1, "q", TOKEN_NEQ));
                    case 'o': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "t", TOKEN_NOT));
                    case 't': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "est", TOKEN_NTEST));
                    case 'u': return asm_lexer_emit(lexer, CLASSIFY(2, 2, "ll", TOKEN_NULL));
                }
            }
        case 'o': return asm_lexer_emit(lexer, CLASSIFY(1, 2, "bj", TOKEN_OBJ));
        case 'p': return asm_lexer_emit(lexer, CLASSIFY(1, 3, "arr", TOKEN_PARR));
        case 'r': return asm_lexer_emit(lexer, CLASSIFY(1, 2, "et", TOKEN_RET));
        case 's':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "t", TOKEN_SET));
                    case 'g': return asm_lexer_emit(lexer, CLASSIFY(2, 3, "lob", TOKEN_SGLOB));
                    case 'h': return asm_lexer_emit(lexer, CLASSIFY(2, 1, "l", TOKEN_SHL) == TOKEN_SHL ? TOKEN_SHL :
                                                           CLASSIFY(2, 1, "r", TOKEN_SHR));
                    case 't': return asm_lexer_emit(lexer, CLASSIFY(2, 4, "ring", TOKEN_STRING_TYPE));
                    case 'u':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'b': return asm_lexer_emit(lexer, TOKEN_SUB);
                                case 'p': return asm_lexer_emit(lexer, CLASSIFY(3, 3, "val", TOKEN_SUPVAL));
                                default: return asm_lexer_emit(lexer, TOKEN_IDENTIFIER);
                            }
                        }
                }
            }
        case 't': return asm_lexer_emit(lexer, CLASSIFY(1, 3, "est", TOKEN_TEST));
        case 'u': return asm_lexer_emit(lexer, CLASSIFY(1, 4, "pval", TOKEN_UPVAL));
    }

    return asm_lexer_emit(lexer, TOKEN_IDENTIFIER);

    #undef CLASSIFY
}

Token asm_next_number(Lexer* lexer) {
    TokenType type = TOKEN_INT;

    while(!AT_END(0) && asm_is_digit(PEEK(0)))
        ADVANCE(1);

    switch(PEEK(0)) {
        case '.':
            type = TOKEN_FLOAT;

            if(!AT_END(1) && asm_is_digit(PEEK(1))) {
                ADVANCE(1);

                while(!AT_END(0) && asm_is_digit(PEEK(0)))
                    ADVANCE(1);

                if(!AT_END(0) && PEEK(0) == 'f')
                    ADVANCE(1);
            } else return asm_lexer_error(lexer, "Expected digit after dot");
            break;
        case 'b':
            type = TOKEN_BYTE;
            ADVANCE(1);
            break;
        case 'c':
            type = TOKEN_CONSTANT;
            ADVANCE(1);
            break;
        case 'f':
            type = TOKEN_FLOAT;
            ADVANCE(1);
            break;
        case 'r':
            type = TOKEN_REGISTER;
            ADVANCE(1);
            break;
    }

    return asm_lexer_emit(lexer, type);
}

Token asm_next_string(Lexer* lexer) {
    while(!AT_END(0)) {
        if(PEEK(0) == '"') {
            if(PEEK(-1) != '\\')
                break;
        } else if(PEEK(0) == '\n')
            return asm_lexer_error(lexer, "Expected end of string");

        ADVANCE(1);
    }

    if(AT_END(0))
        return asm_lexer_error(lexer, "Expected end of string");

    ADVANCE(1);
    return asm_lexer_emit(lexer, TOKEN_STRING);
}

#undef PEEK
#undef ADVANCE
#undef NEXT

#undef AT_END
#undef MATCH
