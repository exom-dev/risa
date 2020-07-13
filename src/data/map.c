#include "map.h"
#include "../memory/mem.h"

#include <stdlib.h>
#include <string.h>

#define MAP_MAX_LOAD 0.75
#define MAP_START_SIZE 8

static Entry* map_find_bucket(Entry* entries, int capacity, ValString* key);
static void map_adjust_capacity(Map* map);

void map_init(Map* map) {
    map->count = 0;
    map->capacity = 0;
    map->entries = NULL;
}

void map_delete(Map* map) {
    MEM_FREE(map->entries);
    map_init(map);
}

uint32_t map_hash(const char* chars, uint32_t length) {
    uint32_t hash = 2166136261u;

    for (uint32_t i = 0; i < length; i++) {
        hash ^= chars[i];
        hash *= 16777619;
    }

    return hash;
}

bool map_get(Map* map, ValString *key, Value* value) {
    if(map->count == 0)
        return false;

    Entry* entry = map_find_bucket(map->entries, map->capacity, key);

    if(entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}

bool map_set(Map* map, ValString *key, Value value) {
    map_adjust_capacity(map);

    Entry* entry = map_find_bucket(map->entries, map->capacity, key);

    bool isNewKey = entry->key == NULL;
    if(isNewKey && IS_NULL(entry->value))
        ++map->count;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

bool map_erase(Map* map, ValString *key) {
    if(map->count == 0)
        return false;

    Entry* entry = map_find_bucket(map->entries, map->capacity, key);
    if(entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VALUE(false);

    return true;
}

void map_copy(Map* map, Map* from) {
    for(int i = 0; i < from->capacity; ++i) {
        Entry* entry = &from->entries[i];
        if(entry->key != NULL)
            map_set(map, entry->key, entry->value);
    }
}

ValString* map_find(Map* map, const char* chars, int length, uint32_t hash) {
    if(map->count == 0)
        return NULL;

    uint32_t index = hash & (map->capacity - 1);

    while(1) {
        Entry* entry = &map->entries[index];

        if(entry->key == NULL) {
            if(IS_NULL(entry->value))
                return NULL;
        } else if(entry->key->length == length
               && entry->key->hash == hash
               && memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }

        index = (index + 1) & (map->capacity - 1);
    }
}

static Entry* map_find_bucket(Entry* entries, int capacity, ValString* key) {
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    while(1) {
        Entry* entry = &entries[index];

        if(entry->key == NULL) {
            if(IS_NULL(entry->value)) {
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

static void map_adjust_capacity(Map* map) {
    if(map->count + 1 > map->capacity * MAP_MAX_LOAD) {
        uint32_t capacity = map->capacity < MAP_START_SIZE ? MAP_START_SIZE : 2 * map->capacity;
        Entry* entries = MEM_ALLOC(capacity * sizeof(Entry));

        for(uint32_t i = 0; i < capacity; ++i) {
            entries[i].key = NULL;
            entries[i].value = NULL_VALUE;
        }

        map->count = 0;
        for(uint32_t i = 0; i < map->capacity; ++i) {
            Entry *entry = &map->entries[i];
            if (entry->key == NULL)
                continue;

            Entry *dest = map_find_bucket(entries, capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;

            ++map->count;
        }

        MEM_FREE(map->entries);
        map->entries = entries;
        map->capacity = capacity;
    }
}