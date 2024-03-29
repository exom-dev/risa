#include "map.h"
#include "../mem/mem.h"
#include "../value/dense.h"

#include <stdlib.h>
#include <string.h>

#define MAP_MAX_LOAD 0.75
#define MAP_START_SIZE 8

static RisaMapEntry* risa_map_find_bucket     (RisaMapEntry* entries, int capacity, RisaDenseStringPtr key);
static void          risa_map_adjust_capacity (RisaMap* map);

void risa_map_init(RisaMap* map) {
    map->count = 0;
    map->capacity = 0;
    map->entries = NULL;
}

void risa_map_delete(RisaMap* map) {
    if(map->entries != NULL)
        RISA_MEM_FREE(map->entries);

    risa_map_init(map);
}

//  FNV-1a hash
uint32_t risa_map_hash(const char* chars, uint32_t length) {
    uint32_t hash = 2166136261u;

    for (uint64_t i = 0; i < length; i++) {
        hash ^= chars[(uint32_t) i];
        hash *= 16777619;
    }

    return hash;
}

bool risa_map_get(RisaMap* map, RisaDenseStringPtr key, RisaValue* value) {
    if(map->count == 0)
        return false;

    RisaMapEntry* entry = risa_map_find_bucket(map->entries, map->capacity, key);

    if(entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}

bool risa_map_set(RisaMap* map, RisaDenseStringPtr key, RisaValue value) {
    risa_map_adjust_capacity(map);

    RisaMapEntry* entry = risa_map_find_bucket(map->entries, map->capacity, key);

    bool isNewKey = entry->key == NULL;
    if(isNewKey && risa_value_is_null(entry->value))
        ++map->count;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

bool risa_map_erase(RisaMap* map, RisaDenseStringPtr key) {
    if(map->count == 0)
        return false;

    RisaMapEntry* entry = risa_map_find_bucket(map->entries, map->capacity, key);
    if(entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = risa_value_from_bool(false);

    return true;
}

void risa_map_copy(RisaMap* dest, RisaMap* src) {
    for(uint32_t i = 0; i < src->capacity; ++i) {
        RisaMapEntry* entry = &src->entries[i];

        if(entry->key != NULL)
            risa_map_set(dest, entry->key, entry->value);
    }
}

RisaDenseStringPtr risa_map_find(RisaMap* map, const char* chars, int length, uint32_t hash) {
    if(map->count == 0)
        return NULL;

    uint32_t index = hash & (map->capacity - 1);

    while(1) {
        RisaMapEntry* entry = &map->entries[index];

        if(entry->key == NULL) {
            if(risa_value_is_null(entry->value))
                return NULL;
        } else if(((RisaDenseString*) (entry->key))->length == length
               && ((RisaDenseString*) (entry->key))->hash == hash
               && memcmp(((RisaDenseString*) (entry->key))->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) & (map->capacity - 1);
    }
}

RisaMapEntry* risa_map_find_entry(RisaMap* map, const char* chars, int length, uint32_t hash) {
    if(map->count == 0)
        return NULL;

    uint32_t index = hash & (map->capacity - 1);

    while(1) {
        RisaMapEntry* entry = &map->entries[index];

        if(entry->key == NULL) {
            if(risa_value_is_null(entry->value))
                return NULL;
        } else if(((RisaDenseString*) (entry->key))->length == length
                  && ((RisaDenseString*) (entry->key))->hash == hash
                  && memcmp(((RisaDenseString*) (entry->key))->chars, chars, length) == 0) {
            return entry;
        }

        index = (index + 1) & (map->capacity - 1);
    }
}

static RisaMapEntry* risa_map_find_bucket(RisaMapEntry* entries, int capacity, RisaDenseStringPtr key) {
    uint32_t index = ((RisaDenseString*) key)->hash & (capacity - 1);
    RisaMapEntry* tombstone = NULL;

    while(1) {
        RisaMapEntry* entry = &entries[index];

        if(entry->key == NULL) {
            if(risa_value_is_null(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if(tombstone == NULL)
                    tombstone = entry;
            }
        } else if(entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

static void risa_map_adjust_capacity(RisaMap* map) {
    if(map->count + 1 > map->capacity * MAP_MAX_LOAD) {
        uint32_t capacity = map->capacity < MAP_START_SIZE ? MAP_START_SIZE : 2 * map->capacity;
        RisaMapEntry* entries = RISA_MEM_ALLOC(capacity * sizeof(RisaMapEntry));

        for(uint32_t i = 0; i < capacity; ++i) {
            entries[i].key = NULL;
            entries[i].value = risa_value_from_null();
        }

        map->count = 0;
        for(uint32_t i = 0; i < map->capacity; ++i) {
            RisaMapEntry* entry = &map->entries[i];

            if(entry->key == NULL)
                continue;

            RisaMapEntry* dest = risa_map_find_bucket(entries, capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;

            ++map->count;
        }

        RISA_MEM_FREE(map->entries);
        map->entries = entries;
        map->capacity = capacity;
    }
}