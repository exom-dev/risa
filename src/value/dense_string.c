#include "dense.h"

#include "../mem/mem.h"
#include "../data/map.h"

#include <string.h>

uint32_t risa_dense_string_hash(RisaDenseString* string) {
    return risa_map_hash(string->chars, string->length);
}

RisaDenseString* risa_dense_string_prepare(const char* chars, uint32_t length) {
    RisaDenseString* string = RISA_MEM_ALLOC(sizeof(RisaDenseString) + length + 1);

    string->dense.type = RISA_DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;

    string->length = length;

    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    return string;
}

RisaDenseString* risa_dense_string_from(const char* chars, uint32_t length) {
    RisaDenseString* string = risa_dense_string_prepare(chars, length);

    risa_dense_string_hash_inplace(string);

    return string;
}

RisaDenseString* risa_dense_string_concat(RisaDenseString* left, RisaDenseString* right) {
    uint32_t length = left->length + right->length;

    RisaDenseString* string = RISA_MEM_ALLOC(sizeof(RisaDenseString) + length + 1);

    string->dense.type = RISA_DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;
    
    string->length = length;

    memcpy(string->chars, left->chars, left->length);
    memcpy(string->chars + left->length, right->chars, right->length);
    string->chars[length] = '\0';

    risa_dense_string_hash_inplace(string);

    return string;
}

void risa_dense_string_hash_inplace(RisaDenseString* string) {
    string->hash = risa_dense_string_hash(string);
}

void risa_dense_string_delete(RisaDenseString* string) {
    RISA_MEM_FREE(string);
}