#include "lexer.h"

#include "../memory/mem.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (++lexer->index, lexer->current += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0')
#define MATCH(c) \
        ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true)) \

bool is_digit(char c);
bool is_alpha(char c);

Token next_identifier(Lexer* lexer);
Token next_number(Lexer* lexer);
Token next_string(Lexer* lexer);

void lexer_init(Lexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->index = 0;
}

void lexer_source(Lexer* lexer, const char* src) {
    lexer->source = src;
    lexer->start = src;
    lexer->current = src;
}

void lexer_delete(Lexer* lexer) {
    lexer_init(lexer);
}

Token lexer_next(Lexer* lexer) {
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
                        return lexer_error(lexer, "Expected end of comment block");

                    ADVANCE(2);
                } else whitespace = false;

                break;
            default:
                whitespace = false;
        }
    }

    lexer->start = lexer->current;

    if(AT_END(0))
        return lexer_emit(lexer, TOKEN_EOF);

    char c = NEXT();

    if(is_alpha(c))
        return next_identifier(lexer);
    if(is_digit(c))
        return next_number(lexer);

    switch(c) {
        case '(': return lexer_emit(lexer, TOKEN_LEFT_PAREN);
        case ')': return lexer_emit(lexer, TOKEN_RIGHT_PAREN);
        case '[': return lexer_emit(lexer, TOKEN_LEFT_BRACKET);
        case ']': return lexer_emit(lexer, TOKEN_RIGHT_BRACKET);
        case '{': return lexer_emit(lexer, TOKEN_LEFT_BRACE);
        case '}': return lexer_emit(lexer, TOKEN_RIGHT_BRACE);
        case ':': return lexer_emit(lexer, TOKEN_COLON);
        case ';': return lexer_emit(lexer, TOKEN_SEMICOLON);
        case ',': return lexer_emit(lexer, TOKEN_COMMA);
        case '.': return lexer_emit(lexer, TOKEN_DOT);
        case '-': return lexer_emit(lexer, TOKEN_MINUS);
        case '+': return lexer_emit(lexer, TOKEN_PLUS);
        case '/': return lexer_emit(lexer, TOKEN_SLASH);
        case '*': return lexer_emit(lexer, TOKEN_STAR);
        case '~': return lexer_emit(lexer, TOKEN_TILDE);
        case '^': return lexer_emit(lexer, TOKEN_CARET);
        case '%': return lexer_emit(lexer, TOKEN_PERCENT);
        case '!': return lexer_emit(lexer, MATCH('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return lexer_emit(lexer, MATCH('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return lexer_emit(lexer, MATCH('=') ? TOKEN_LESS_EQUAL :
                                           MATCH('<') ? TOKEN_LESS_LESS : TOKEN_LESS);
        case '>': return lexer_emit(lexer, MATCH('=') ? TOKEN_GREATER_EQUAL :
                                           MATCH('>') ? TOKEN_GREATER_GREATER : TOKEN_GREATER);
        case '&': return lexer_emit(lexer, MATCH('&') ? TOKEN_AMPERSAND_AMPERSAND : TOKEN_AMPERSAND);
        case '|': return lexer_emit(lexer, MATCH('|') ? TOKEN_PIPE_PIPE : TOKEN_PIPE);
        case '"': return next_string(lexer);

        default: return lexer_error(lexer, "Unexpected character");
    }
}

Token lexer_emit(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.size = (size_t) (lexer->current - lexer->start);
    token.index = lexer->index - token.size;

    return token;
}

Token lexer_error(Lexer* lexer, const char* msg) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = msg;
    token.size = strlen(msg);
    token.index = lexer->index;

    return token;
}

bool identifier_equals(Token* left, Token* right) {
    if(left->size != right->size)
        return false;
    return memcmp(left->start, right->start, left->size) == 0;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c == '_');
}

Token next_identifier(Lexer* lexer) {
    #define CLASSIFY(index, length, str, type) \
        ((lexer->current - lexer->start == index + length) && \
         (memcmp(lexer->start + index, str, length) == 0)) ? type : TOKEN_IDENTIFIER

    while(!AT_END(0) && (is_alpha(PEEK(0)) || is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'b': return lexer_emit(lexer, CLASSIFY(1, 4, "reak", TOKEN_BREAK));
        case 'c': return lexer_emit(lexer, CLASSIFY(1, 7, "ontinue", TOKEN_CONTINUE));
        case 'e': return lexer_emit(lexer, CLASSIFY(1, 3, "lse", TOKEN_ELSE));
        case 'f':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': return lexer_emit(lexer, CLASSIFY(2, 3, "lse", TOKEN_FALSE));
                    case 'o': return lexer_emit(lexer, CLASSIFY(2, 1, "r", TOKEN_FOR));
                    case 'u': return lexer_emit(lexer, CLASSIFY(2, 6, "nction", TOKEN_FUNCTION));
                }
            }
            break;
        case 'i': return lexer_emit(lexer, CLASSIFY(1, 1, "f", TOKEN_IF));
        case 'n': return lexer_emit(lexer, CLASSIFY(1, 3, "ull", TOKEN_NULL));
        case 'r': return lexer_emit(lexer, CLASSIFY(1, 5, "eturn", TOKEN_RETURN));
        case 't': return lexer_emit(lexer, CLASSIFY(1, 3, "rue", TOKEN_TRUE));
        case 'v': return lexer_emit(lexer, CLASSIFY(1, 2, "ar", TOKEN_VAR));
        case 'w': return lexer_emit(lexer, CLASSIFY(1, 4, "hile", TOKEN_WHILE));
    }

    return lexer_emit(lexer, TOKEN_IDENTIFIER);

    #undef CLASSIFY
}

Token next_number(Lexer* lexer) {
    TokenType type = TOKEN_INT;

    while(!AT_END(0) && is_digit(PEEK(0)))
        ADVANCE(1);

    if(PEEK(0) == '.') {
        type = TOKEN_FLOAT;

        if(!AT_END(1) && is_digit(PEEK(1))) {
            ADVANCE(1);

            while(!AT_END(0) && is_digit(PEEK(0)))
                ADVANCE(1);
        } else return lexer_error(lexer, "Expected digit after dot");
    }

    if(PEEK(0) == 'b') {
        type = TOKEN_BYTE;
        ADVANCE(1);
    }

    return lexer_emit(lexer, type);
}

Token next_string(Lexer* lexer) {
    while(!AT_END(0)) {
        if(PEEK(0) == '"') {
            if(PEEK(-1) != '\\')
                break;
        } else if(PEEK(0) == '\n')
            return lexer_error(lexer, "Expecter end of string");

        ADVANCE(1);
    }

    if(AT_END(0))
        return lexer_error(lexer, "Expecter end of string");

    ADVANCE(1);
    return lexer_emit(lexer, TOKEN_STRING);
}

#undef PEEK
#undef ADVANCE
#undef NEXT

#undef AT_END
#undef MATCH