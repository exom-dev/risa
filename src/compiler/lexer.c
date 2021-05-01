#include "lexer.h"

#include "../lib/charlib.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (lexer->index += i, lexer->current += i)
#define IGNORE(i) (lexer->index += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0')
#define MATCH(c) \
        ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true)) \

static RisaToken risa_lexer_next_identifier(RisaLexer* lexer);
static RisaToken risa_lexer_next_number(RisaLexer* lexer);
static RisaToken risa_lexer_next_string(RisaLexer* lexer);

void risa_lexer_init(RisaLexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->index = 0;
}

void risa_lexer_source(RisaLexer* lexer, const char* src) {
    lexer->source = src;
    lexer->start = src;
    lexer->current = src;
}

void risa_lexer_delete(RisaLexer* lexer) {
    risa_lexer_init(lexer);
}

RisaToken risa_lexer_next(RisaLexer* lexer) {
    lexer->start = lexer->source + lexer->index;
    lexer->current = lexer->start;

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
                        return risa_lexer_error(lexer, "Expected end of comment block");

                    ADVANCE(2);
                } else whitespace = false;

                break;
            default:
                whitespace = false;
        }
    }

    lexer->start = lexer->source + lexer->index;
    lexer->current = lexer->start;

    if(AT_END(0))
        return risa_lexer_emit(lexer, RISA_TOKEN_EOF);

    char c = NEXT();

    if(risa_lib_charlib_is_alphascore(c))
        return risa_lexer_next_identifier(lexer);
    if(risa_lib_charlib_is_digit(c))
        return risa_lexer_next_number(lexer);

    switch(c) {
        case '(': return risa_lexer_emit(lexer, RISA_TOKEN_LEFT_PAREN);
        case ')': return risa_lexer_emit(lexer, RISA_TOKEN_RIGHT_PAREN);
        case '[': return risa_lexer_emit(lexer, RISA_TOKEN_LEFT_BRACKET);
        case ']': return risa_lexer_emit(lexer, RISA_TOKEN_RIGHT_BRACKET);
        case '{': return risa_lexer_emit(lexer, RISA_TOKEN_LEFT_BRACE);
        case '}': return risa_lexer_emit(lexer, RISA_TOKEN_RIGHT_BRACE);
        case ':': return risa_lexer_emit(lexer, RISA_TOKEN_COLON);
        case ';': return risa_lexer_emit(lexer, RISA_TOKEN_SEMICOLON);
        case ',': return risa_lexer_emit(lexer, RISA_TOKEN_COMMA);
        case '.': return risa_lexer_emit(lexer, RISA_TOKEN_DOT);
        case '-': return risa_lexer_emit(lexer, MATCH('-') ? RISA_TOKEN_MINUS_MINUS :
                                                MATCH('=') ? RISA_TOKEN_MINUS_EQUAL : RISA_TOKEN_MINUS);
        case '+': return risa_lexer_emit(lexer, MATCH('+') ? RISA_TOKEN_PLUS_PLUS :
                                                MATCH('=') ? RISA_TOKEN_PLUS_EQUAL : RISA_TOKEN_PLUS);
        case '/': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_SLASH_EQUAL : RISA_TOKEN_SLASH);
        case '*': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_STAR_EQUAL : RISA_TOKEN_STAR);
        case '~': return risa_lexer_emit(lexer, RISA_TOKEN_TILDE);
        case '^': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_CARET_EQUAL : RISA_TOKEN_CARET);
        case '%': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_PERCENT_EQUAL : RISA_TOKEN_PERCENT);
        case '?': return risa_lexer_emit(lexer, RISA_TOKEN_QUESTION);
        case '$': return risa_lexer_emit(lexer, RISA_TOKEN_DOLLAR);
        case '!': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_BANG_EQUAL : RISA_TOKEN_BANG);
        case '=': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_EQUAL_EQUAL :
                                                MATCH('>') ? RISA_TOKEN_EQUAL_GREATER : RISA_TOKEN_EQUAL);
        case '<': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_LESS_EQUAL :
                                                MATCH('<') ? RISA_TOKEN_LESS_LESS : RISA_TOKEN_LESS);
        case '>': return risa_lexer_emit(lexer, MATCH('=') ? RISA_TOKEN_GREATER_EQUAL :
                                                MATCH('>') ? RISA_TOKEN_GREATER_GREATER : RISA_TOKEN_GREATER);
        case '&': return risa_lexer_emit(lexer, MATCH('&') ? RISA_TOKEN_AMPERSAND_AMPERSAND :
                                                MATCH('=') ? RISA_TOKEN_AMPERSAND_EQUAL : RISA_TOKEN_AMPERSAND);
        case '|': return risa_lexer_emit(lexer, MATCH('|') ? RISA_TOKEN_PIPE_PIPE :
                                                MATCH('=') ? RISA_TOKEN_PIPE_EQUAL : RISA_TOKEN_PIPE);
        case '"': return risa_lexer_next_string(lexer);

        default: return risa_lexer_error(lexer, "Unexpected character");
    }
}

RisaToken risa_lexer_emit(RisaLexer* lexer, RisaTokenType type) {
    RisaToken token;
    token.type = type;
    token.start = lexer->start;
    token.size = (size_t) (lexer->current - lexer->start);
    token.index = (uint32_t) ((lexer->current - token.size) - lexer->source);

    return token;
}

RisaToken risa_lexer_error(RisaLexer* lexer, const char* msg) {
    RisaToken token;
    token.type = RISA_TOKEN_ERROR;
    token.start = msg;
    token.size = strlen(msg);
    token.index = lexer->index;

    return token;
}

bool risa_token_identifier_equals(RisaToken* left, RisaToken* right) {
    if(left->size != right->size)
        return false;
    return memcmp(left->start, right->start, left->size) == 0;
}

static RisaToken risa_lexer_next_identifier(RisaLexer* lexer) {
    #define CLASSIFY(index, length, str, type) \
        ((lexer->current - lexer->start == index + length) && \
         (memcmp(lexer->start + index, str, length) == 0)) ? type : RISA_TOKEN_IDENTIFIER

    while(!AT_END(0) && (risa_lib_charlib_is_alphascore(PEEK(0)) || risa_lib_charlib_is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'b': return risa_lexer_emit(lexer, CLASSIFY(1, 4, "reak", RISA_TOKEN_BREAK));
        case 'c':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'o': return risa_lexer_emit(lexer, CLASSIFY(2, 6, "ntinue", RISA_TOKEN_CONTINUE));
                    case 'l': return risa_lexer_emit(lexer, CLASSIFY(2, 3, "one", RISA_TOKEN_CLONE));
                }
            }
            break;
        case 'e': return risa_lexer_emit(lexer, CLASSIFY(1, 3, "lse", RISA_TOKEN_ELSE));
        case 'f':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return risa_lexer_emit(lexer, CLASSIFY(2, 3, "lse", RISA_TOKEN_FALSE));
                    case 'o': return risa_lexer_emit(lexer, CLASSIFY(2, 1, "r", RISA_TOKEN_FOR));
                    case 'u': return risa_lexer_emit(lexer, CLASSIFY(2, 6, "nction", RISA_TOKEN_FUNCTION));
                }
            }
            break;
        case 'i': return risa_lexer_emit(lexer, CLASSIFY(1, 1, "f", RISA_TOKEN_IF));
        case 'n': return risa_lexer_emit(lexer, CLASSIFY(1, 3, "ull", RISA_TOKEN_NULL));
        case 'r': return risa_lexer_emit(lexer, CLASSIFY(1, 5, "eturn", RISA_TOKEN_RETURN));
        case 't': return risa_lexer_emit(lexer, CLASSIFY(1, 3, "rue", RISA_TOKEN_TRUE));
        case 'v': return risa_lexer_emit(lexer, CLASSIFY(1, 2, "ar", RISA_TOKEN_VAR));
        case 'w': return risa_lexer_emit(lexer, CLASSIFY(1, 4, "hile", RISA_TOKEN_WHILE));
    }

    return risa_lexer_emit(lexer, RISA_TOKEN_IDENTIFIER);

    #undef CLASSIFY
}

static RisaToken risa_lexer_next_number(RisaLexer* lexer) {
    RisaTokenType type = RISA_TOKEN_INT;

    while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
        ADVANCE(1);

    switch(PEEK(0)) {
        case '.':
            type = RISA_TOKEN_FLOAT;

            if(!AT_END(1) && risa_lib_charlib_is_digit(PEEK(1))) {
                ADVANCE(1);

                while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
                    ADVANCE(1);

                if(!AT_END(0) && PEEK(0) == 'f')
                    IGNORE(1);
            } else return risa_lexer_error(lexer, "Expected digit after dot");
            break;
        case 'b':
            type = RISA_TOKEN_BYTE;
            IGNORE(1);
            break;
        case 'f':
            type = RISA_TOKEN_FLOAT;
            IGNORE(1);
            break;
    }

    return risa_lexer_emit(lexer, type);
}

static RisaToken risa_lexer_next_string(RisaLexer* lexer) {
    while(!AT_END(0)) {
        if(PEEK(0) == '"') {
            if(PEEK(-1) != '\\')
                break;
        } else if(PEEK(0) == '\n')
            return risa_lexer_error(lexer, "Expected end of string");

        ADVANCE(1);
    }

    if(AT_END(0))
        return risa_lexer_error(lexer, "Expected end of string");

    ADVANCE(1);
    return risa_lexer_emit(lexer, RISA_TOKEN_STRING);
}

#undef PEEK
#undef IGNORE
#undef ADVANCE
#undef NEXT

#undef AT_END
#undef MATCH