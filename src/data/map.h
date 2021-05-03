#ifndef RISA_DATA_MAP_H_GUARD
#define RISA_DATA_MAP_H_GUARD

#include "../api.h"
#include "../value/value.h"

typedef void* RisaDenseStringPtr;

typedef struct {
    RisaDenseStringPtr key;
    RisaValue value;
} RisaMapEntry;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    RisaMapEntry* entries;
} RisaMap;

RISA_API void               risa_map_init            (RisaMap* map);
RISA_API void               risa_map_delete          (RisaMap* map);

RISA_API uint32_t           risa_map_hash            (const char* chars, uint32_t length);

RISA_API bool               risa_map_get             (RisaMap* map, RisaDenseStringPtr key, RisaValue* value);
RISA_API bool               risa_map_set             (RisaMap* map, RisaDenseStringPtr key, RisaValue value);
RISA_API bool               risa_map_erase           (RisaMap* map, RisaDenseStringPtr key);
RISA_API void               risa_map_copy            (RisaMap* map, RisaMap* from);

RISA_API RisaDenseStringPtr risa_map_find            (RisaMap* map, const char* chars, int length, uint32_t hash);
RISA_API RisaMapEntry*      risa_map_find_entry      (RisaMap* map, const char* chars, int length, uint32_t hash);

RISA_API RisaDenseStringPtr risa_map_entry_get_key   (RisaMapEntry* entry);
RISA_API RisaValue          risa_map_entry_get_value (RisaMapEntry* entry);

#endif
