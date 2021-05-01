#ifndef RISA_DATA_MAP_H_GUARD
#define RISA_DATA_MAP_H_GUARD

#include "../value/value.h"

typedef void* DenseStringPtr;

typedef struct {
    DenseStringPtr key;
    RisaValue value;
} RisaMapEntry;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    RisaMapEntry* entries;
} RisaMap;

void           risa_map_init       (RisaMap* map);
void           risa_map_delete     (RisaMap* map);

uint32_t       risa_map_hash       (const char* chars, uint32_t length);

bool           risa_map_get        (RisaMap* map, DenseStringPtr key, RisaValue* value);
bool           risa_map_set        (RisaMap* map, DenseStringPtr key, RisaValue value);
bool           risa_map_erase      (RisaMap* map, DenseStringPtr key);
void           risa_map_copy       (RisaMap* map, RisaMap* from);

DenseStringPtr risa_map_find       (RisaMap* map, const char* chars, int length, uint32_t hash);
RisaMapEntry*  risa_map_find_entry (RisaMap* map, const char* chars, int length, uint32_t hash);

#endif
