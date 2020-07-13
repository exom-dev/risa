#include "value.h"

#include "../memory/mem.h"
#include "../data/map.h"

#include <string.h>

uint32_t value_string_hash(ValString* string) {
    return map_hash(string->chars, string->length);
}

ValString* value_string_from(const char* chars, uint32_t length) {
    ValString* string = MEM_ALLOC(sizeof(ValString) + length + 1);

    string->link.type = LVAL_STRING;
    string->link.next = NULL;
    string->length = length;

    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    string->hash = value_string_hash(string);

    return string;
}

ValString* value_string_concat(ValString* left, ValString* right) {
    uint32_t length = left->length + right->length;

    ValString* string = MEM_ALLOC(sizeof(ValString) + length + 1);

    string->link.type = LVAL_STRING;
    string->link.next = NULL;
    string->length = length;

    memcpy(string->chars, left->chars, left->length);
    memcpy(string->chars + left->length, right->chars, right->length);
    string->chars[length] = '\0';

    string->hash = value_string_hash(string);

    return string;
}