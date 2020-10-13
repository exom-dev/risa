#include "dense.h"

#include "../memory/mem.h"
#include "../data/map.h"

#include <string.h>

uint32_t dense_string_hash(DenseString* string) {
    return map_hash(string->chars, string->length);
}

DenseString* dense_string_from(const char* chars, uint32_t length) {
    DenseString* string = MEM_ALLOC(sizeof(DenseString) + length + 1);

    string->dense.type = DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;

    string->length = length;

    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    string->hash = dense_string_hash(string);

    return string;
}

DenseString* dense_string_concat(DenseString* left, DenseString* right) {
    uint32_t length = left->length + right->length;

    DenseString* string = MEM_ALLOC(sizeof(DenseString) + length + 1);

    string->dense.type = DVAL_STRING;
    string->dense.link = NULL;
    string->dense.marked = false;
    
    string->length = length;

    memcpy(string->chars, left->chars, left->length);
    memcpy(string->chars + left->length, right->chars, right->length);
    string->chars[length] = '\0';

    string->hash = dense_string_hash(string);

    return string;
}

void dense_string_delete(DenseString* string) {
    MEM_FREE(string);
}