#include "cluster.h"

#include "../mem/mem.h"

void cluster_init(RisaCluster* cluster) {
    cluster->size = 0;
    cluster->capacity = 0;
    cluster->bytecode = NULL;
    cluster->indices = NULL;

    value_array_init(&cluster->constants);
}

void cluster_write(RisaCluster* cluster, uint8_t byte, uint32_t index) {
    while(cluster->capacity <= cluster->size) {
        cluster->bytecode = (uint8_t*) RISA_MEM_EXPAND(cluster->bytecode, &cluster->capacity, sizeof(uint8_t));
        cluster->indices = (uint32_t*) RISA_MEM_REALLOC(cluster->indices, cluster->capacity, sizeof(uint32_t));
    }

    cluster->bytecode[cluster->size] = byte;
    cluster->indices[cluster->size] = index;
    ++cluster->size;
}

size_t cluster_write_constant(RisaCluster* cluster, RisaValue constant) {
    for(uint32_t i = 0; i < cluster->constants.size; ++i)
        if(value_strict_equals(constant, cluster->constants.values[i]))
            return i;

    value_array_write(&cluster->constants, constant);
    return cluster->constants.size - 1;
}

void cluster_delete(RisaCluster* cluster) {
    RISA_MEM_FREE(cluster->bytecode);
    RISA_MEM_FREE(cluster->indices);

    value_array_delete(&cluster->constants);

    cluster_init(cluster);
}
