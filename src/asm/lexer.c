#include "lexer.h"

#include "../lib/charlib.h"

#include <string.h>

#define PEEK(i) lexer->current[i]
#define ADVANCE(i) (++lexer->index, lexer->current += i)
#define NEXT() (++lexer->index, *(lexer->current++))

#define AT_END(i) (lexer->current[i] == '\0' || (lexer->stoppers != NULL && (!lexer->ignoreStoppers && strchr(lexer->stoppers, lexer->current[i]) != NULL)))
#define MATCH(c) \
    ((AT_END(0) || *lexer->current != c) ? false : (ADVANCE(1), true))

AsmToken asm_next_identifier(AsmLexer* lexer);
AsmToken asm_next_number(AsmLexer* lexer);
AsmToken asm_next_string(AsmLexer* lexer);

OpCode asm_token_to_opcode(AsmTokenType token) {
    switch(token) {
        case ASM_TOKEN_CNST   : return OP_CNST   ;
        case ASM_TOKEN_CNSTW  : return OP_CNSTW  ;
        case ASM_TOKEN_MOV    : return OP_MOV    ;
        case ASM_TOKEN_CLONE  : return OP_CLONE  ;
        case ASM_TOKEN_DGLOB  : return OP_DGLOB  ;
        case ASM_TOKEN_GGLOB  : return OP_GGLOB  ;
        case ASM_TOKEN_SGLOB  : return OP_SGLOB  ;
        case ASM_TOKEN_UPVAL  : return OP_UPVAL  ;
        case ASM_TOKEN_GUPVAL : return OP_GUPVAL ;
        case ASM_TOKEN_SUPVAL : return OP_SUPVAL ;
        case ASM_TOKEN_CUPVAL : return OP_CUPVAL ;
        case ASM_TOKEN_CLSR   : return OP_CLSR   ;
        case ASM_TOKEN_ARR    : return OP_ARR    ;
        case ASM_TOKEN_PARR   : return OP_PARR   ;
        case ASM_TOKEN_LEN    : return OP_LEN    ;
        case ASM_TOKEN_OBJ    : return OP_OBJ    ;
        case ASM_TOKEN_GET    : return OP_GET    ;
        case ASM_TOKEN_SET    : return OP_SET    ;
        case ASM_TOKEN_NULL   : return OP_NULL   ;
        case ASM_TOKEN_TRUE   : return OP_TRUE   ;
        case ASM_TOKEN_FALSE  : return OP_FALSE  ;
        case ASM_TOKEN_NOT    : return OP_NOT    ;
        case ASM_TOKEN_BNOT   : return OP_BNOT   ;
        case ASM_TOKEN_NEG    : return OP_NEG    ;
        case ASM_TOKEN_INC    : return OP_INC    ;
        case ASM_TOKEN_DEC    : return OP_DEC    ;
        case ASM_TOKEN_ADD    : return OP_ADD    ;
        case ASM_TOKEN_SUB    : return OP_SUB    ;
        case ASM_TOKEN_MUL    : return OP_MUL    ;
        case ASM_TOKEN_DIV    : return OP_DIV    ;
        case ASM_TOKEN_MOD    : return OP_MOD    ;
        case ASM_TOKEN_SHL    : return OP_SHL    ;
        case ASM_TOKEN_SHR    : return OP_SHR    ;
        case ASM_TOKEN_LT     : return OP_LT     ;
        case ASM_TOKEN_LTE    : return OP_LTE    ;
        case ASM_TOKEN_EQ     : return OP_EQ     ;
        case ASM_TOKEN_NEQ    : return OP_NEQ    ;
        case ASM_TOKEN_BAND   : return OP_BAND   ;
        case ASM_TOKEN_BXOR   : return OP_BXOR   ;
        case ASM_TOKEN_BOR    : return OP_BOR    ;
        case ASM_TOKEN_TEST   : return OP_TEST   ;
        case ASM_TOKEN_NTEST  : return OP_NTEST  ;
        case ASM_TOKEN_JMP    : return OP_JMP    ;
        case ASM_TOKEN_JMPW   : return OP_JMPW   ;
        case ASM_TOKEN_BJMP   : return OP_BJMP   ;
        case ASM_TOKEN_BJMPW  : return OP_BJMPW  ;
        case ASM_TOKEN_CALL   : return OP_CALL   ;
        case ASM_TOKEN_RET    : return OP_RET    ;
        case ASM_TOKEN_ACC    : return OP_ACC    ;
        case ASM_TOKEN_DIS    : return OP_DIS    ;
        default               : return OP_CNST   ;
    }
}

void asm_lexer_init(AsmLexer* lexer) {
    lexer->source = NULL;
    lexer->start = NULL;
    lexer->current = NULL;
    lexer->stoppers = NULL;
    lexer->ignoreStoppers = false;
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

    if(risa_lib_charlib_is_alphascore(c))
        return asm_next_identifier(lexer);
    if(risa_lib_charlib_is_digit(c))
        return asm_next_number(lexer);

    switch(c) {
        case '.': return asm_lexer_emit(lexer, ASM_TOKEN_DOT);
        case '(': return asm_lexer_emit(lexer, ASM_TOKEN_LEFT_PAREN);
        case ')': return asm_lexer_emit(lexer, ASM_TOKEN_RIGHT_PAREN);
        case '{': return asm_lexer_emit(lexer, ASM_TOKEN_LEFT_BRACE);
        case '}': return asm_lexer_emit(lexer, ASM_TOKEN_RIGHT_BRACE);
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

AsmToken asm_next_identifier(AsmLexer* lexer) {
    #define CLASSIFY_INSENS(index, length, str, type) \
        (((lexer->current - lexer->start == index + length) && \
        (risa_lib_charlib_strnicmp(lexer->start + index, str, length))) ? type : ASM_TOKEN_IDENTIFIER)

    while(!AT_END(0) && (risa_lib_charlib_is_alphascore(PEEK(0)) || risa_lib_charlib_is_digit(PEEK(0))))
        ADVANCE(1);

    switch(*lexer->start) {
        case 'a': case 'A':
            if(lexer->current - lexer->start > 1) {
                switch(lexer->start[1]) {
                    case 'c': case 'C': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "c", ASM_TOKEN_ACC));
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
                    case 'i': case 'I': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "v", ASM_TOKEN_DIV) == ASM_TOKEN_DIV ? ASM_TOKEN_DIV :
                                                                     CLASSIFY_INSENS(2, 1, "s", ASM_TOKEN_DIS));
                    case 'g': case 'G': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 3, "lob", ASM_TOKEN_DGLOB));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'e': case 'E': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 1, "q", ASM_TOKEN_EQ));
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
                    case 'u': case 'U': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 4, "pval", ASM_TOKEN_GUPVAL));
                    default: return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
                }
            } else return asm_lexer_emit(lexer, ASM_TOKEN_IDENTIFIER);
        case 'i': case 'I': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 2, "nc", ASM_TOKEN_INC) == ASM_TOKEN_INC ? ASM_TOKEN_INC :
                                                         CLASSIFY_INSENS(1, 2, "nt", ASM_TOKEN_INT_TYPE));
        case 'j': case 'J': return asm_lexer_emit(lexer, CLASSIFY_INSENS(1, 3, "mpw", ASM_TOKEN_JMPW) == ASM_TOKEN_JMPW ? ASM_TOKEN_JMPW :
                                                         CLASSIFY_INSENS(1, 2, "mp", ASM_TOKEN_JMP));
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
                    case 'o': case 'O': return asm_lexer_emit(lexer, CLASSIFY_INSENS(2, 1, "v", ASM_TOKEN_MOV) == ASM_TOKEN_MOV ? ASM_TOKEN_MOV :
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

    while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
        ADVANCE(1);

    switch(PEEK(0)) {
        case '.':
            type = ASM_TOKEN_FLOAT;

            if(!AT_END(1) && risa_lib_charlib_is_digit(PEEK(1))) {
                ADVANCE(1);

                while(!AT_END(0) && risa_lib_charlib_is_digit(PEEK(0)))
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
