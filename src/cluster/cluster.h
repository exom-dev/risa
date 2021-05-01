#ifndef RISA_CLUSTER_H
#define RISA_CLUSTER_H

#include "../def/types.h"
#include "../value/value.h"

typedef struct {
    size_t size;
    size_t capacity;

    uint8_t* bytecode;
    uint32_t* indices;

    RisaValueArray constants;
} RisaCluster;

void   risa_cluster_init           (RisaCluster* cluster);
void   risa_cluster_write          (RisaCluster* cluster, uint8_t byte, uint32_t index);
size_t risa_cluster_write_constant (RisaCluster* cluster, RisaValue constant);
void   risa_cluster_delete         (RisaCluster* cluster);

#endif
