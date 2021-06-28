#include "lexer.h"

#include "../lib/charlib.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (lexer->index += i, lexer->current += i)
#define IGNORE(i) (lexer->index += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0' || (lexer->stoppers != NULL && (!lexer->ignoreStoppers && strchr(lexer->stoppers, lexer->current[i]) != NULL)))
#define MATCH(c) \
    ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true))

static RisaAsmToken risa_asm_lexer_next_identifier (RisaAsmLexer* lexer);
static RisaAsmToken risa_asm_lexer_next_number     (RisaAsmLexer* lexer);
static RisaAsmToken risa_asm_lexer_next_string     (RisaAsmLexer* lexer);

RisaOpCode risa_asm_token_to_opcode(RisaAsmTokenType token) {
    switch(token) {
        case RISA_ASM_TOKEN_CNST   : return RISA_OP_CNST   ;
        case RISA_ASM_TOKEN_CNSTW  : return RISA_OP_CNSTW  ;
        case RISA_ASM_TOKEN_MOV    : return RISA_OP_MOV    ;
        case RISA_ASM_TOKEN_CLONE  : return RISA_OP_CLONE  ;
        case RISA_ASM_TOKEN_DGLOB  : return RISA_OP_DGLOB  ;
        case RISA_ASM_TOKEN_GGLOB  : return RISA_OP_GGLOB  ;
        case RISA_ASM_TOKEN_SGLOB  : return RISA_OP_SGLOB  ;
        case RISA_ASM_TOKEN_UPVAL  : return RISA_OP_UPVAL  ;
        case RISA_ASM_TOKEN_GUPVAL : return RISA_OP_GUPVAL ;
        case RISA_ASM_TOKEN_SUPVAL : return RISA_OP_SUPVAL ;
        case RISA_ASM_TOKEN_CUPVAL : return RISA_OP_CUPVAL ;
        case RISA_ASM_TOKEN_CLSR   : return RISA_OP_CLSR   ;
        case RISA_ASM_TOKEN_ARR    : return RISA_OP_ARR    ;
        case RISA_ASM_TOKEN_PARR   : return RISA_OP_PARR   ;
        case RISA_ASM_TOKEN_LEN    : return RISA_OP_LEN    ;
        case RISA_ASM_TOKEN_OBJ    : return RISA_OP_OBJ    ;
        case RISA_ASM_TOKEN_GET    : return RISA_OP_GET    ;
        case RISA_ASM_TOKEN_SET    : return RISA_OP_SET    ;
        case RISA_ASM_TOKEN_NULL   : return RISA_OP_NULL   ;
        case RISA_ASM_TOKEN_TRUE   : return RISA_OP_TRUE   ;
        case RISA_ASM_TOKEN_FALSE  : return RISA_OP_FALSE  ;
        case RISA_ASM_TOKEN_NOT    : return RISA_OP_NOT    ;
        case RISA_ASM_TOKEN_BNOT   : return RISA_OP_BNOT   ;
        case RISA_ASM_TOKEN_NEG    : return RISA_OP_NEG    ;
        case RISA_ASM_TOKEN_INC    : return RISA_OP_INC    ;
        case RISA_ASM_TOKEN_DEC    : return RISA_OP_DEC    ;
        case RISA_ASM_TOKEN_ADD    : return RISA_OP_ADD    ;
        case RISA_ASM_TOKEN_SUB    : return RISA_OP_SUB    ;
        case RISA_ASM_TOKEN_MUL    : return RISA_OP_MUL    ;
        case RISA_ASM_TOKEN_DIV    : return RISA_OP_DIV    ;
        case RISA_ASM_TOKEN_MOD    : return RISA_OP_MOD    ;
        case RISA_ASM_TOKEN_SHL    : return RISA_OP_SHL    ;
        case RISA_ASM_TOKEN_SHR    : return RISA_OP_SHR    ;
        case RISA_ASM_TOKEN_LT     : return RISA_OP_LT     ;
        case RISA_ASM_TOKEN_LTE    : return RISA_OP_LTE    ;
        case RISA_ASM_TOKEN_EQ     : return RISA_OP_EQ     ;
        case RISA_ASM_TOKEN_NEQ    : return RISA_OP_NEQ    ;
        case RISA_ASM_TOKEN_BAND   : return RISA_OP_BAND   ;
        case RISA_ASM_TOKEN_BXOR   : return RISA_OP_BXOR   ;
        case RISA_ASM_TOKEN_BOR    : return RISA_OP_BOR    ;
        case RISA_ASM_TOKEN_TEST   : return RISA_OP_TEST   ;
        case RISA_ASM_TOKEN_NTEST  : return RISA_OP_NTEST  ;
        case RISA_ASM_TOKEN_JMP    : return RISA_OP_JMP    ;
        case RISA_ASM_TOKEN_JMPW   : return RISA_OP_JMPW   ;
        case RISA_ASM_TOKEN_BJMP   : return RISA_OP_BJMP   ;
        case RISA_ASM_TOKEN_BJMPW  : return RISA_OP_BJMPW  ;
        case RISA_ASM_TOKEN_CALL   : return RISA_OP_CALL   ;
        case RISA_ASM_TOKEN_RET    : return RISA_OP_RET    ;
        case RISA_ASM_TOKEN_ACC    : return RISA_OP_ACC    ;
        case RISA_ASM_TOKEN_DIS    : return RISA_OP_DIS    ;
        default               : return RISA_OP_CNST   ;
    }
}

void risa_asm_lexer_init(RisaAsmLexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->stoppers = NULL;
    lexer->ignoreStoppers = false;
    lexer->index = 0;
}

void risa_asm_lexer_source(RisaAsmLexer* lexer, const char* src) {
    lexer->source = src;
    lexer->start = src;
    lexer->current = src;
}

void risa_asm_lexer_delete(RisaAsmLexer* lexer) {
    risa_asm_lexer_init(lexer);
}

RisaAsmToken risa_asm_lexer_next(RisaAsmLexer* lexer) {
    lexer->start = lexer->source + lexer->index;
    lexer->current = lexer->start;

    bool whitespace = true;

    while(!AT_END(0) && whitespace) {
        switch(PEEK(0)) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case ';':
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
                        return risa_asm_lexer_error(lexer, "Expected end of comment block");

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
        return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_EOF);

    char c = NEXT();

    if(risa_lib_charlib_is_alphascore(c))
        return risa_asm_lexer_next_identifier(lexer);
    if(risa_lib_charlib_is_digit(c))
        return risa_asm_lexer_next_number(lexer);

    switch(c) {
        case '.': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_DOT);
        case '(': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_LEFT_PAREN);
        case ')': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_RIGHT_PAREN);
        case '{': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_LEFT_BRACE);
        case '}': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_RIGHT_BRACE);
        case '"': return risa_asm_lexer_next_string(lexer);
        default:
            // The index needs to point to the character, so decrement the index;
            --lexer->index;
            RisaAsmToken errorToken = risa_asm_lexer_error(lexer, "Unexpected character");
            ++lexer->index;

            return errorToken;
    }
}

RisaAsmToken risa_asm_lexer_emit(RisaAsmLexer* lexer, RisaAsmTokenType type) {
    RisaAsmToken token;
    token.type = type;
    token.start = lexer->start;
    token.size = (size_t) (lexer->current - lexer->start);
    token.index = lexer->index - token.size;

    return token;
}

RisaAsmToken risa_asm_lexer_error(RisaAsmLexer* lexer, const char* msg) {
    RisaAsmToken token;
    token.type = RISA_ASM_TOKEN_ERROR;
    token.start = msg;
    token.size = strlen(msg);
    token.index = lexer->index;

    return token;
}

static RisaAsmToken risa_asm_lexer_next_identifier(RisaAsmLexer* lexer) {
    #define CLASSIFY_INSENS(index, length, str, type) \
        (((lexer->current - lexer->start == index + length) && \
        (risa_lib_charlib_strnicmp(lexer->start + index, str, length))) ? type : RISA_ASM_TOKEN_IDENTIFIER)

    while(!AT_END(0) && (risa_lib_charlib_is_alphascore(PEEK(0)) || risa_lib_charlib_is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'a': case 'A':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'c': case 'C': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "c", RISA_ASM_TOKEN_ACC));
                    case 'd': case 'D': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "d", RISA_ASM_TOKEN_ADD));
                    case 'r': case 'R': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "r", RISA_ASM_TOKEN_ARR));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'b': case 'B':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "nd", RISA_ASM_TOKEN_BAND));
                    case 'j': case 'J': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "mpw", RISA_ASM_TOKEN_BJMPW) == RISA_ASM_TOKEN_BJMPW ? RISA_ASM_TOKEN_BJMPW :
                                                                          CLASSIFY_INSENS(2, 2, "mp", RISA_ASM_TOKEN_BJMP));
                    case 'n': case 'N': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ot", RISA_ASM_TOKEN_BNOT));
                    case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "r", RISA_ASM_TOKEN_BOR) == RISA_ASM_TOKEN_BOR ? RISA_ASM_TOKEN_BOR :
                                                                          CLASSIFY_INSENS(2, 2, "ol", RISA_ASM_TOKEN_BOOL_TYPE));
                    case 'x': case 'X': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "or", RISA_ASM_TOKEN_BXOR));
                    case 'y': case 'Y': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "te", RISA_ASM_TOKEN_BYTE_TYPE));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'c': case 'C':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ll", RISA_ASM_TOKEN_CALL));
                    case 'l': case 'L':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 2, "ne", RISA_ASM_TOKEN_CLONE));
                                case 's': case 'S': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 1, "r", RISA_ASM_TOKEN_CLSR));
                            }
                        }
                    case 'n': case 'N': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "stw", RISA_ASM_TOKEN_CNSTW) == RISA_ASM_TOKEN_CNSTW ? RISA_ASM_TOKEN_CNSTW :
                                                                          CLASSIFY_INSENS(2, 2, "st", RISA_ASM_TOKEN_CNST));
                    case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "de", RISA_ASM_TOKEN_CODE));
                    case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "pval", RISA_ASM_TOKEN_CUPVAL));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'd': case 'D':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ta", RISA_ASM_TOKEN_DATA));
                    case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "c", RISA_ASM_TOKEN_DEC));
                    case 'i': case 'I': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "v", RISA_ASM_TOKEN_DIV) == RISA_ASM_TOKEN_DIV ? RISA_ASM_TOKEN_DIV :
                                                                          CLASSIFY_INSENS(2, 1, "s", RISA_ASM_TOKEN_DIS));
                    case 'g': case 'G': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", RISA_ASM_TOKEN_DGLOB));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 1, "q", RISA_ASM_TOKEN_EQ));
        case 'f': case 'F':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'a': case 'A': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lse", RISA_ASM_TOKEN_FALSE));
                    case 'l': case 'L': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "oat", RISA_ASM_TOKEN_FLOAT_TYPE));
                    case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 6, "nction", RISA_ASM_TOKEN_FUNCTION_TYPE));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 4, "alse", RISA_ASM_TOKEN_FALSE));
        case 'g': case 'G':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", RISA_ASM_TOKEN_GET));
                    case 'g': case 'G': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", RISA_ASM_TOKEN_GGLOB));
                    case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "pval", RISA_ASM_TOKEN_GUPVAL));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'i': case 'I': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "nc", RISA_ASM_TOKEN_INC) == RISA_ASM_TOKEN_INC ? RISA_ASM_TOKEN_INC :
                                                              CLASSIFY_INSENS(1, 2, "nt", RISA_ASM_TOKEN_INT_TYPE));
        case 'j': case 'J': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "mpw", RISA_ASM_TOKEN_JMPW) == RISA_ASM_TOKEN_JMPW ? RISA_ASM_TOKEN_JMPW :
                                                              CLASSIFY_INSENS(1, 2, "mp", RISA_ASM_TOKEN_JMP));
        case 'l': case 'L':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "n", RISA_ASM_TOKEN_LEN));
                    case 't': case 'T': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "e", RISA_ASM_TOKEN_LTE) == RISA_ASM_TOKEN_LTE ? RISA_ASM_TOKEN_LTE : RISA_ASM_TOKEN_LT);
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'm': case 'M':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "v", RISA_ASM_TOKEN_MOV) == RISA_ASM_TOKEN_MOV ? RISA_ASM_TOKEN_MOV :
                                                                          CLASSIFY_INSENS(2, 1, "d", RISA_ASM_TOKEN_MOD));
                    case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "l", RISA_ASM_TOKEN_MUL));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'n': case 'N':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "g", RISA_ASM_TOKEN_NEG) == RISA_ASM_TOKEN_NEG ? RISA_ASM_TOKEN_NEG :
                                                                          CLASSIFY_INSENS(2, 1, "q", RISA_ASM_TOKEN_NEQ));
                    case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", RISA_ASM_TOKEN_NOT));
                    case 't': case 'T': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "est", RISA_ASM_TOKEN_NTEST));
                    case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 2, "ll", RISA_ASM_TOKEN_NULL));
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 'o': case 'O': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "bj", RISA_ASM_TOKEN_OBJ));
        case 'p': case 'P': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "arr", RISA_ASM_TOKEN_PARR));
        case 'r': case 'R': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "et", RISA_ASM_TOKEN_RET));
        case 's': case 'S':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'e': case 'E': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "t", RISA_ASM_TOKEN_SET));
                    case 'g': case 'G': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", RISA_ASM_TOKEN_SGLOB));
                    case 'h': case 'H': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "l", RISA_ASM_TOKEN_SHL) == RISA_ASM_TOKEN_SHL ? RISA_ASM_TOKEN_SHL :
                                                                          CLASSIFY_INSENS(2, 1, "r", RISA_ASM_TOKEN_SHR));
                    case 't': case 'T': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "ring", RISA_ASM_TOKEN_STRING_TYPE));
                    case 'u': case 'U':
                        if(lexer->current - lexer->start > 2) {
                            switch(lexer->start[2]) {
                                case 'b': case 'B': return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_SUB);
                                case 'p': case 'P': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(3, 3, "val", RISA_ASM_TOKEN_SUPVAL));
                                default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                            }
                        } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                    default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
                }
            } else return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
        case 't': case 'T': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "est", RISA_ASM_TOKEN_TEST));
        case 'u': case 'U': return risa_asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 4, "pval", RISA_ASM_TOKEN_UPVAL));
        default: return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_IDENTIFIER);
    }

    #undef CLASSIFY
}

static RisaAsmToken risa_asm_lexer_next_number(RisaAsmLexer* lexer) {
    RisaAsmTokenType type = RISA_ASM_TOKEN_INT;

    while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
        ADVANCE(1);

    switch(PEEK(0)) {
        case '.':
            type = RISA_ASM_TOKEN_FLOAT;

            if(!AT_END(1) && risa_lib_charlib_is_digit(PEEK(1))) {
                ADVANCE(1);

                while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
                    ADVANCE(1);

                if((!AT_END(0) && PEEK(0) == 'f') || (!AT_END(0) && PEEK(0) == 'F'))
                    IGNORE(1);
            } else return risa_asm_lexer_error(lexer, "Expected digit after dot");
            break;
        case 'b': case 'B':
            type = RISA_ASM_TOKEN_BYTE;
            IGNORE(1);
            break;
        case 'c': case 'C':
            type = RISA_ASM_TOKEN_CONSTANT;
            IGNORE(1);
            break;
        case 'f': case 'F':
            type = RISA_ASM_TOKEN_FLOAT;
            IGNORE(1);
            break;
        case 'r': case 'R':
            type = RISA_ASM_TOKEN_REGISTER;
            IGNORE(1);
            break;
    }

    return risa_asm_lexer_emit(lexer, type);
}

static RisaAsmToken risa_asm_lexer_next_string(RisaAsmLexer* lexer) {
    while(!AT_END(0)) {
        if(PEEK(0) == '"') {
            if(PEEK(-1) != '\\')
                break;
        } else if(PEEK(0) == '\n')
            return risa_asm_lexer_error(lexer, "Expected end of string");

        ADVANCE(1);
    }

    if(AT_END(0))
        return risa_asm_lexer_error(lexer, "Expected end of string");

    ADVANCE(1);
    return risa_asm_lexer_emit(lexer, RISA_ASM_TOKEN_STRING);
}

#undef PEEK
#undef ADVANCE
#undef NEXT

#undef AT_END
#undef MATCH
