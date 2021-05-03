#include "map.h"
#include "../mem/mem.h"
#include "../value/dense.h"

RisaDenseStringPtr risa_map_entry_get_key(RisaMapEntry* entry) {
    return entry->key;
}

RisaValue risa_map_entry_get_value(RisaMapEntry* entry) {
    return entry->value;
}