#ifndef RISA_DATA_MAP_H_GUARD
#define RISA_DATA_MAP_H_GUARD

#include "../value/value.h"
#include "../value/dense.h"

typedef struct {
    DenseString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Map;

void map_init(Map* map);
void map_delete(Map* map);

uint32_t map_hash(const char* chars, uint32_t length);

bool map_get(Map* map, DenseString* key, Value* value);
bool map_set(Map* map, DenseString* key, Value value);
bool map_erase(Map* map, DenseString* key);
void map_copy(Map* map, Map* from);

DenseString* map_find(Map* map, const char* chars, int length, uint32_t hash);

#endif
