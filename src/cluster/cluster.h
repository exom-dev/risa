#ifndef RISA_CLUSTER_H
#define RISA_CLUSTER_H

#include "../api.h"
#include "../def/types.h"
#include "../value/value.h"

typedef struct {
    uint32_t size;
    uint32_t capacity;

    uint8_t* bytecode;
    uint32_t* indices;

    RisaValueArray constants;
} RisaCluster;

RISA_API RisaCluster* risa_cluster_create             ();
RISA_API void         risa_cluster_init               (RisaCluster* cluster);
RISA_API void         risa_cluster_write              (RisaCluster* cluster, uint8_t byte, uint32_t index);
RISA_API uint32_t     risa_cluster_write_constant     (RisaCluster* cluster, RisaValue constant);
RISA_API uint32_t     risa_cluster_get_size           (RisaCluster* cluster);
RISA_API uint8_t      risa_cluster_get_data_at        (RisaCluster* cluster, uint32_t index);
RISA_API uint32_t     risa_cluster_get_constant_count (RisaCluster* cluster);
RISA_API RisaValue    risa_cluster_get_constant_at    (RisaCluster* cluster, uint32_t index);
RISA_API void         risa_cluster_init               (RisaCluster* cluster);
RISA_API void         risa_cluster_clone              (RisaCluster* dest, RisaCluster* src);
RISA_API void         risa_cluster_delete             (RisaCluster* cluster);
RISA_API void         risa_cluster_free               (RisaCluster* cluster);

#endif
