#include "dense.h"

#include "../memory/mem.h"
#include "../data/map.h"

#include <string.h>

uint32_t dense_string_hash(DenseString* string) {
    return map_hash(string->chars, string->length);
}

DenseString* dense_string_prepare(const char* chars, uint32_t length) {
    DenseString* string = RISA_MEM_ALLOC(sizeof(DenseString) + length + 1);

    string->dense.type = DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;

    string->length = length;

    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    return string;
}

DenseString* dense_string_from(const char* chars, uint32_t length) {
    DenseString* string = dense_string_prepare(chars, length);

    dense_string_hash_inplace(string);

    return string;
}

DenseString* dense_string_concat(DenseString* left, DenseString* right) {
    uint32_t length = left->length + right->length;

    DenseString* string = RISA_MEM_ALLOC(sizeof(DenseString) + length + 1);

    string->dense.type = DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;
    
    string->length = length;

    memcpy(string->chars, left->chars, left->length);
    memcpy(string->chars + left->length, right->chars, right->length);
    string->chars[length] = '\0';

    dense_string_hash_inplace(string);

    return string;
}

void dense_string_hash_inplace(DenseString* string) {
    string->hash = dense_string_hash(string);
}

void dense_string_delete(DenseString* string) {
    RISA_MEM_FREE(string);
}