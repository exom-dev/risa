#include "cluster.h"

#include "../mem/mem.h"

RisaCluster* risa_cluster_create() {
    RisaCluster* cluster = RISA_MEM_ALLOC(sizeof(RisaCluster));

    risa_cluster_init(cluster);

    return cluster;
}

void risa_cluster_init(RisaCluster* cluster) {
    cluster->size = 0;
    cluster->capacity = 0;
    cluster->bytecode = NULL;
    cluster->indices = NULL;

    risa_value_array_init(&cluster->constants);
}

void risa_cluster_reserve(RisaCluster* cluster, uint32_t capacity) {
    if(cluster->capacity >= capacity) {
        return;
    }

    uint32_t target = cluster->capacity + capacity;

    while(cluster->capacity <= target) {
        cluster->bytecode = (uint8_t*) RISA_MEM_EXPAND(cluster->bytecode, &cluster->capacity, sizeof(uint8_t));
        cluster->indices = (uint32_t*) RISA_MEM_REALLOC(cluster->indices, cluster->capacity, sizeof(uint32_t));
    }
}

void risa_cluster_write(RisaCluster* cluster, uint8_t byte, uint32_t index) {
    while(cluster->capacity <= cluster->size) {
        cluster->bytecode = (uint8_t*) RISA_MEM_EXPAND(cluster->bytecode, &cluster->capacity, sizeof(uint8_t));
        cluster->indices = (uint32_t*) RISA_MEM_REALLOC(cluster->indices, cluster->capacity, sizeof(uint32_t));
    }

    cluster->bytecode[cluster->size] = byte;
    cluster->indices[cluster->size] = index;
    ++cluster->size;
}

uint32_t risa_cluster_write_constant(RisaCluster* cluster, RisaValue constant) {
    for(uint32_t i = 0; i < cluster->constants.size; ++i)
        if(risa_value_strict_equals(constant, cluster->constants.values[i]))
            return i;

    risa_value_array_write(&cluster->constants, constant);
    return cluster->constants.size - 1;
}

uint32_t risa_cluster_get_size(RisaCluster* cluster) {
    return cluster->size;
}

uint8_t risa_cluster_get_data_at(RisaCluster* cluster, uint32_t index) {
    return cluster->bytecode[index];
}

uint32_t risa_cluster_get_constant_count(RisaCluster* cluster) {
    return cluster->constants.size;
}

RisaValue risa_cluster_get_constant_at(RisaCluster* cluster, uint32_t index) {
    return cluster->constants.values[index];
}

void risa_cluster_clone(RisaCluster* dest, RisaCluster* src) {
    for(uint32_t i = 0; i < src->size; ++i) {
        risa_cluster_write(dest, src->bytecode[i], src->indices[i]);
    }

    for(uint32_t i = 0; i < src->constants.size; ++i) {
        risa_cluster_write_constant(dest, src->constants.values[i]);
    }
}

void risa_cluster_delete(RisaCluster* cluster) {
    RISA_MEM_FREE(cluster->bytecode);
    RISA_MEM_FREE(cluster->indices);

    risa_value_array_delete(&cluster->constants);

    risa_cluster_init(cluster);
}

void risa_cluster_free(RisaCluster* cluster) {
    risa_cluster_delete(cluster);

    RISA_MEM_FREE(cluster);
}
