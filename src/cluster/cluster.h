#ifndef RISA_CLUSTER_H
#define RISA_CLUSTER_H

#include "../api.h"
#include "../def/types.h"
#include "../value/value.h"
#include "../data/map.h"
#include "../data/buffer.h"

#define RISA_CLUSTER_MAGIC "RCLS"
#define RISA_CLUSTER_ENDIANNESS_TEST 0x2200FF44

typedef struct {
    uint32_t size;
    uint32_t capacity;

    uint8_t* bytecode;
    uint32_t* indices;

    RisaValueArray constants;
} RisaCluster;

typedef struct {
    RisaMap strings;

    RisaBuffer output;
    RisaBuffer stringsBuffer;
    uint32_t stringCount;
} RisaClusterSerializer;

typedef struct {
    RisaConstBuffer input;

    struct {
        RisaDenseStringPtr* data;
        uint32_t capacity;
        uint32_t size;  // Number of bytes
        uint32_t count; // Number of strings
    } strings;

    void* vm;
    RisaCluster output;
} RisaClusterDeserializer;

typedef enum {
    RISA_DESERIALIZATION_OK,
    RISA_DESERIALIZATION_ERROR_MAGIC_MISMATCH,
    RISA_DESERIALIZATION_ERROR_ENDIANNESS_MISMATCH,
    RISA_DESERIALIZATION_ERROR_VERSION_MISMATCH,
    RISA_DESERIALIZATION_ERROR_EOF,
    RISA_DESERIALIZATION_ERROR_OTHER
} RisaClusterDeserializationStatus;

RISA_API RisaCluster*                      risa_cluster_create                       ();
RISA_API void                              risa_cluster_init                         (RisaCluster* cluster);
RISA_API void                              risa_cluster_reserve                      (RisaCluster* cluster, uint32_t capacity);
RISA_API void                              risa_cluster_write                        (RisaCluster* cluster, uint8_t byte, uint32_t index);
RISA_API uint32_t                          risa_cluster_write_constant               (RisaCluster* cluster, RisaValue constant);
RISA_API uint32_t                          risa_cluster_get_size                     (RisaCluster* cluster);
RISA_API uint8_t                           risa_cluster_get_data_at                  (RisaCluster* cluster, uint32_t index);
RISA_API uint32_t                          risa_cluster_get_constant_count           (RisaCluster* cluster);
RISA_API RisaValue                         risa_cluster_get_constant_at              (RisaCluster* cluster, uint32_t index);
RISA_API void                              risa_cluster_clone                        (RisaCluster* dest, RisaCluster* src);
RISA_API void                              risa_cluster_delete                       (RisaCluster* cluster);
RISA_API void                              risa_cluster_free                         (RisaCluster* cluster);

RISA_API void                              risa_cluster_serializer_init              (RisaClusterSerializer* serializer);
RISA_API void                              risa_cluster_serializer_delete            (RisaClusterSerializer* serializer);
RISA_API RisaBuffer*                       risa_cluster_serializer_serialize         (RisaClusterSerializer* serializer, RisaCluster* cluster);
RISA_API void                              risa_cluster_serializer_delete_serialized (RisaBuffer* serialized);

RISA_API void                              risa_cluster_deserializer_init            (RisaClusterDeserializer* deserializer);
RISA_API void                              risa_cluster_deserializer_delete          (RisaClusterDeserializer* deserializer);
RISA_API void                              risa_cluster_deserializer_target          (RisaClusterDeserializer* deserializer, void* vm); // Avoid including the vm in the header.
RISA_API RisaClusterDeserializationStatus  risa_cluster_deserializer_deserialize     (RisaClusterDeserializer* deserializer, const uint8_t* input, uint32_t size);

#endif
