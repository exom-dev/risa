#include "lexer.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (++lexer->index, lexer->current += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0' || (lexer->stoppers != NULL && strchr(lexer->stoppers, lexer->current[i]) != NULL))
#define MATCH(c) \
    ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true))

static bool asm_is_digit(char);
static bool asm_is_alpha(char);
static bool asm_stricmp(const char*, const char*, size_t);

AsmToken asm_next_identifier(AsmLexer* lexer);
AsmToken asm_next_number(AsmLexer* lexer);
AsmToken asm_next_string(AsmLexer* lexer);

void asm_lexer_init(AsmLexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->stoppers = NULL;
    lexer->index = 0;
}

void asm_lexer_source(AsmLexer* lexer, const char* src) {
    lexer->source = src;
    lexer->start = src;
    lexer->current = src;
}

void asm_lexer_delete(AsmLexer* lexer) {
    asm_lexer_init(lexer);
}

AsmToken asm_lexer_next(AsmLexer* lexer) {
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
        return asm_lexer_emit(lexer, ASM_TOKEN_EOF);

    char c = NEXT();

    if(asm_is_alpha(c))
        return asm_next_identifier(lexer);
    if(asm_is_digit(c))
        return asm_next_number(lexer);

    switch(c) {
        case '.': return asm_lexer_emit(lexer, ASM_TOKEN_DOT);
        case ',': return asm_lexer_emit(lexer, ASM_TOKEN_COMMA);
        case '"': return asm_next_string(lexer);
        default: return asm_lexer_error(lexer, "Unexpected character");
    }
}

AsmToken asm_lexer_emit(AsmLexer* lexer, AsmTokenType type) {
    AsmToken token;
    token.type = type;
    token.start = lexer->start;
    token.size = (size_t) (lexer->current - lexer->start);
    token.index = lexer->index - token.size;

    return token;
}

AsmToken asm_lexer_error(AsmLexer* lexer, const char* msg) {
    AsmToken token;
    token.type = ASM_TOKEN_ERROR;
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

static bool asm_stricmp(const char* left, const char* right, size_t length) {
    #define LWR(c) (c >= 'A' && c <= 'Z' ? c + 32 : c)

    const char* leftLimit = left + length;

    while(LWR(*left) == LWR(*right) && left < leftLimit) {
        ++left;
        ++right;
    }

    return left == leftLimit;

    #undef LWR
}

AsmToken asm_next_identifier(AsmLexer* lexer) {
    #define CLASSIFY_INSENS(index, length, str, type) \
        (((lexer->current - lexer->start == index + length) && \
        (asm_stricmp(lexer->start + index, str, length))) ? type : ASM_TOKEN_IDENTIFIER)

    while(!AT_END(0) && (asm_is_alpha(PEEK(0)) || asm_is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'a': case 'A':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'd': case 'D': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "d", ASM_TOKEN_ADD));
                    case 'r': case 'R': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "r", ASM_TOKEN_ARR));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'b': case 'B':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "nd", ASM_TOKEN_BAND));
                    case 'j': case 'J': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "mpw", ASM_TOKEN_BJMPW) == ASM_TOKEN_BJMPW ? ASM_TOKEN_BJMPW :
                                                                     CLASSIFY_INSENS(2, 2, "mp", ASM_TOKEN_BJMP));
                    case 'n': case 'N': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ot", ASM_TOKEN_BNOT));
                    case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "r", ASM_TOKEN_BOR) == ASM_TOKEN_BOR ? ASM_TOKEN_BOR :
                                                                     CLASSIFY_INSENS(2, 2, "ol", ASM_TOKEN_BOOL_TYPE));
                    case 'x': case 'X': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "or", ASM_TOKEN_BXOR));
                    case 'y': case 'Y': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "te", ASM_TOKEN_BYTE_TYPE));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'c': case 'C':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ll", ASM_TOKEN_CALL));
                    case 'l': case 'L':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 2, "ne", ASM_TOKEN_CLONE));
                                case 's': case 'S': return asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 1, "r", ASM_TOKEN_CLSR));
                            }
                        }
                    case 'n': case 'N': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "stw", ASM_TOKEN_CNSTW) == ASM_TOKEN_CNSTW ? ASM_TOKEN_CNSTW :
                                                                     CLASSIFY_INSENS(2, 2, "st", ASM_TOKEN_CNST));
                    case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "de", ASM_TOKEN_CODE));
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "pval", ASM_TOKEN_CUPVAL));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'd': case 'D':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ta", ASM_TOKEN_DATA));
                    case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "c", ASM_TOKEN_DEC));
                    case 'i': case 'I': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "v", ASM_TOKEN_DIV));
                    case 'g': case 'G': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", ASM_TOKEN_DGLOB));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 1, "e", ASM_TOKEN_EQ));
        case 'f': case 'F':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lse", ASM_TOKEN_FALSE));
                    case 'l': case 'L': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "oat", ASM_TOKEN_FLOAT_TYPE));
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 6, "nction", ASM_TOKEN_FUNCTION_TYPE));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 4, "alse", ASM_TOKEN_FALSE));
        case 'g': case 'G':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", ASM_TOKEN_GET));
                    case 'g': case 'G': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", ASM_TOKEN_GGLOB));
                    case 't': case 'T': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "e", ASM_TOKEN_GTE) == ASM_TOKEN_GTE ? ASM_TOKEN_GTE : ASM_TOKEN_GT);
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "pval", ASM_TOKEN_GUPVAL));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'i': case 'I': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "nc", ASM_TOKEN_INC) == ASM_TOKEN_INC ? ASM_TOKEN_INC :
                                                         CLASSIFY_INSENS(1, 2, "nt", ASM_TOKEN_INT_TYPE));
        case 'j': case 'J': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "mpw", ASM_TOKEN_JMPW) == ASM_TOKEN_JMPW ? ASM_TOKEN_JMPW :
                                                         CLASSIFY_INSENS(2, 2, "mp", ASM_TOKEN_JMP));
        case 'l': case 'L':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "n", ASM_TOKEN_LEN));
                    case 't': case 'T': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "e", ASM_TOKEN_LTE) == ASM_TOKEN_LTE ? ASM_TOKEN_LTE : ASM_TOKEN_LT);
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'm': case 'M':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "d", ASM_TOKEN_MOD) == ASM_TOKEN_MOV ? ASM_TOKEN_MOV :
                                                                     CLASSIFY_INSENS(2, 1, "d", ASM_TOKEN_MOD));
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "l", ASM_TOKEN_MUL));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'n': case 'N':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "g", ASM_TOKEN_NEG) == ASM_TOKEN_NEG ? ASM_TOKEN_NEG :
                                                                     CLASSIFY_INSENS(2, 1, "q", ASM_TOKEN_NEQ));
                    case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", ASM_TOKEN_NOT));
                    case 't': case 'T': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "est", ASM_TOKEN_NTEST));
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ll", ASM_TOKEN_NULL));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "bj", ASM_TOKEN_OBJ));
        case 'p': case 'P': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "arr", ASM_TOKEN_PARR));
        case 'r': case 'R': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "et", ASM_TOKEN_RET));
        case 's': case 'S':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", ASM_TOKEN_SET));
                    case 'g': case 'G': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", ASM_TOKEN_SGLOB));
                    case 'h': case 'H': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "l", ASM_TOKEN_SHL) == ASM_TOKEN_SHL ? ASM_TOKEN_SHL :
                                                                     CLASSIFY_INSENS(2, 1, "r", ASM_TOKEN_SHR));
                    case 't': case 'T': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "ring", ASM_TOKEN_STRING_TYPE));
                    case 'u': case 'U':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'b': case 'B': return asm_lexer_emit(lexer, ASM_TOKEN_SUB);
                                case 'p': case 'P': return asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 3, "val", ASM_TOKEN_SUPVAL));
                                default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                            }
                        } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 't': case 'T': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "est", ASM_TOKEN_TEST));
        case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 4, "pval", ASM_TOKEN_UPVAL));
        default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
    }

    #undef CLASSIFY
}

AsmToken asm_next_number(AsmLexer* lexer) {
    AsmTokenType type = ASM_TOKEN_INT;

    while(!AT_END(0) && asm_is_digit(PEEK(0)))
        ADVANCE(1);

    switch(PEEK(0)) {
        case '.':
            type = ASM_TOKEN_FLOAT;

            if(!AT_END(1) && asm_is_digit(PEEK(1))) {
                ADVANCE(1);

                while(!AT_END(0) && asm_is_digit(PEEK(0)))
                    ADVANCE(1);

                if((!AT_END(0) && PEEK(0) == 'f') || (!AT_END(0) && PEEK(0) == 'F'))
                    ADVANCE(1);
            } else return asm_lexer_error(lexer, "Expected digit after dot");
            break;
        case 'b': case 'B':
            type = ASM_TOKEN_BYTE;
            ADVANCE(1);
            break;
        case 'c': case 'C':
            type = ASM_TOKEN_CONSTANT;
            ADVANCE(1);
            break;
        case 'f': case 'F':
            type = ASM_TOKEN_FLOAT;
            ADVANCE(1);
            break;
        case 'r': case 'R':
            type = ASM_TOKEN_REGISTER;
            ADVANCE(1);
            break;
    }

    return asm_lexer_emit(lexer, type);
}

AsmToken asm_next_string(AsmLexer* lexer) {
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
    return asm_lexer_emit(lexer, ASM_TOKEN_STRING);
}

#undef PEEK
#undef ADVANCE
#undef NEXT

#undef AT_END
#undef MATCH
