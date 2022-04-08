#include "cluster.h"
#include "../data/buffer.h"
#include "../mem/mem.h"
#include "../version.h"
#include "../value/dense.h"

static void risa_cluster_serialize(RisaClusterSerializer* serializer, RisaCluster* cluster);
static void risa_cluster_serialize_value_array(RisaClusterSerializer* serializer, RisaValueArray array);
static void risa_cluster_serialize_value(RisaClusterSerializer* serializer, RisaValue value);

void risa_cluster_serializer_init(RisaClusterSerializer* serializer) {
    risa_map_init(&serializer->strings);
    risa_buffer_init(&serializer->output);
    risa_buffer_init(&serializer->stringsBuffer);
}

void risa_cluster_serializer_delete(RisaClusterSerializer* serializer) {
    risa_map_delete(&serializer->strings);
    risa_buffer_delete(&serializer->output);
    risa_buffer_delete(&serializer->stringsBuffer);

    risa_cluster_serializer_init(serializer);
}

RisaBuffer* risa_cluster_serializer_serialize(RisaClusterSerializer* serializer, RisaCluster* cluster) {
    risa_buffer_write(&serializer->output, (const uint8_t*) RISA_CLUSTER_MAGIC, sizeof(RISA_CLUSTER_MAGIC) - 1);

    risa_buffer_write_uint32(&serializer->output, (uint32_t) RISA_CLUSTER_ENDIANNESS_TEST);
    risa_buffer_write_uint32(&serializer->output, (uint32_t) RISA_VERSION_SIGNATURE);

    // Data + bytecode size, used to jump directly to the string section
    uint32_t dataBytecodeSizeOffset = risa_buffer_write_uint32(&serializer->output, 0);

    risa_cluster_serialize(serializer, cluster);

    // Patch the data + bytecode size and write the strings section
    risa_buffer_patch_size(&serializer->output, dataBytecodeSizeOffset);
    risa_buffer_write_uint32(&serializer->output, serializer->strings.count);
    risa_buffer_write(&serializer->output, serializer->stringsBuffer.data, serializer->stringsBuffer.size);

    risa_buffer_delete(&serializer->stringsBuffer);

    return &serializer->output;
}

void risa_cluster_serializer_delete_serialized(RisaBuffer* serialized) {
    RISA_MEM_FREE(serialized->data);
}

static void risa_cluster_serialize(RisaClusterSerializer* serializer, RisaCluster* cluster) {
    // Constants
    risa_cluster_serialize_value_array(serializer, cluster->constants);

    // Bytecode
    risa_buffer_write_uint32(&serializer->output, cluster->size);
    risa_buffer_write(&serializer->output, cluster->bytecode, cluster->size);
    risa_buffer_write(&serializer->output, (uint8_t*) cluster->indices, sizeof(uint32_t) * cluster->size);
}

static void risa_cluster_serialize_value_array(RisaClusterSerializer* serializer, RisaValueArray array) {
    // This will be patched with the actual size after all values have been written
    //uint32_t dataSizeOffset = risa_buffer_write_uint32(&serializer->output, 0);

    risa_buffer_write_uint32(&serializer->output, array.size);

    for(uint32_t i = 0; i < array.size; ++i) {
        risa_cluster_serialize_value(serializer, array.values[i]);
    }

    // Patch the data size
    //risa_buffer_patch_size(&serializer->output, dataSizeOffset);
}

static void risa_cluster_serialize_value(RisaClusterSerializer* serializer, RisaValue value) {
    // dd dd tt tt -> d - dense type, t - value type
    risa_buffer_write_nibbles(&serializer->output, value_is_dense(value) ? (uint8_t) (risa_value_as_dense(value)->type) : 0, value.type);

    switch(value.type) {
        case RISA_VAL_NULL:
            break;
        case RISA_VAL_BOOL:
            risa_buffer_write_uint8(&serializer->output, (uint8_t) risa_value_as_bool(value));
            break;
        case RISA_VAL_BYTE:
            risa_buffer_write_uint8(&serializer->output, risa_value_as_byte(value));
            break;
        case RISA_VAL_INT:
            risa_buffer_write_int64(&serializer->output, risa_value_as_int(value));
            break;
        case RISA_VAL_FLOAT:
            risa_buffer_write_double(&serializer->output, risa_value_as_float(value));
            break;
        case RISA_VAL_DENSE: {
            switch(risa_value_as_dense(value)->type) {
                case RISA_DVAL_STRING: {
                    RisaDenseString* str = RISA_AS_STRING(value);

                    RisaValue strIndex;
                    if(!risa_map_get(&serializer->strings, str, &strIndex)) {
                        risa_buffer_write_uint32(&serializer->stringsBuffer, str->length);
                        risa_buffer_write(&serializer->stringsBuffer, (uint8_t*) str->chars, str->length);

                        uint32_t index = serializer->strings.count;

                        risa_map_set(&serializer->strings, str, risa_value_from_int(index));
                        risa_buffer_write_uint32(&serializer->output, index);
                    } else {
                        risa_buffer_write_uint32(&serializer->output, (uint32_t) risa_value_as_int(strIndex));
                    }
                    break;
                }
                case RISA_DVAL_ARRAY: {
                    RisaDenseArray* arr = RISA_AS_ARRAY(value);

                    risa_cluster_serialize_value_array(serializer, arr->data);
                    break;
                }
                case RISA_DVAL_OBJECT: {
                    RisaDenseObject* obj = RISA_AS_OBJECT(value);

                    risa_buffer_write_uint32(&serializer->output, obj->data.count);

                    for(int i = 0; i < obj->data.count; ++i) {
                        risa_cluster_serialize_value(serializer, risa_value_from_dense((RisaDenseValue*) (RisaDenseString*) obj->data.entries[i].key));
                        risa_cluster_serialize_value(serializer, obj->data.entries[i].value);
                    }
                    break;
                }
                case RISA_DVAL_UPVALUE: {
                    // This should never be reached
                    break;
                }
                case RISA_DVAL_FUNCTION: {
                    RisaDenseFunction * fn = RISA_AS_FUNCTION(value);

                    risa_cluster_serialize_value(serializer, risa_value_from_dense((RisaDenseValue*) fn->name));
                    risa_buffer_write_uint8(&serializer->output, fn->arity);
                    risa_cluster_serialize(serializer, &fn->cluster);

                    break;
                }
                case RISA_DVAL_CLOSURE: {
                    // This should never be reached
                    break;
                }
                case RISA_DVAL_NATIVE: {
                    // This should never be reached
                    break;
                }
                default:
                    break;
            }
            break;
        }
    }
}