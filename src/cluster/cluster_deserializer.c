#include "cluster.h"
#include "../data/buffer.h"
#include "../mem/mem.h"
#include "../version.h"
#include "../value/dense.h"
#include "../vm/vm.h"

#include "string.h"

static bool risa_cluster_deserialize(RisaClusterDeserializer* deserializer, RisaCluster* cluster);
static bool risa_cluster_deserialize_string(RisaClusterDeserializer* deserializer);
static bool risa_cluster_deserialize_value_array(RisaClusterDeserializer* deserializer, RisaValueArray* array);
static bool risa_cluster_deserialize_value(RisaClusterDeserializer* deserializer, RisaValue* value);

void risa_cluster_deserializer_init(RisaClusterDeserializer* deserializer) {
    risa_cluster_init(&deserializer->output);
    risa_const_buffer_init(&deserializer->input);

    deserializer->strings.data = NULL;
    deserializer->strings.capacity = 0;
    deserializer->strings.size = 0;
    deserializer->strings.count = 0;
}

void risa_cluster_deserializer_delete(RisaClusterDeserializer* deserializer) {
    if(deserializer->strings.data != NULL) {
        RISA_MEM_FREE(deserializer->strings.data);
    }

    risa_cluster_deserializer_init(deserializer);
}

void risa_cluster_deserializer_target(RisaClusterDeserializer* deserializer, void* vm) {
    deserializer->vm = vm;
}

RisaClusterDeserializationStatus risa_cluster_deserializer_deserialize(RisaClusterDeserializer* deserializer, const uint8_t* input, uint32_t size) {
    deserializer->input.data = input;
    deserializer->input.size = size;

    {   // Check compatibility
        uint8_t magic[sizeof(RISA_CLUSTER_MAGIC) - 1];

        if(!risa_const_buffer_read(&deserializer->input, magic, sizeof(RISA_CLUSTER_MAGIC) - 1)) {
            return RISA_DESERIALIZATION_ERROR_EOF;
        }

        // Magic doesn't match
        if(memcmp(magic, RISA_CLUSTER_MAGIC, sizeof(RISA_CLUSTER_MAGIC) - 1) != 0) {
            return RISA_DESERIALIZATION_ERROR_MAGIC_MISMATCH;
        }

        uint32_t endianness;

        if(!risa_const_buffer_read_uint32(&deserializer->input, &endianness)) {
            return RISA_DESERIALIZATION_ERROR_EOF;
        }

        // Endianness doesn't match
        if(endianness != RISA_CLUSTER_ENDIANNESS_TEST) {
            return RISA_DESERIALIZATION_ERROR_ENDIANNESS_MISMATCH;
        }

        uint32_t version;

        if(!risa_const_buffer_read_uint32(&deserializer->input, &version)) {
            return RISA_DESERIALIZATION_ERROR_EOF;
        }

        // Version doesn't match
        if(version != RISA_VERSION_SIGNATURE) {
            return RISA_DESERIALIZATION_ERROR_VERSION_MISMATCH;
        }
    }

    uint32_t stringsOffset;

    if(!risa_const_buffer_read_uint32(&deserializer->input, &stringsOffset)) {
        return RISA_DESERIALIZATION_ERROR_EOF;
    }

    uint32_t clusterStart;

    if(!risa_const_buffer_skip(&deserializer->input, stringsOffset, &clusterStart)) {
        return RISA_DESERIALIZATION_ERROR_EOF;
    }

    {   // Deserialize strings
        if (!risa_const_buffer_read_uint32(&deserializer->input, &deserializer->strings.count)) {
            return RISA_DESERIALIZATION_ERROR_EOF;
        }

        uint32_t count = deserializer->strings.count;

        while (count > 0) {
            if (!risa_cluster_deserialize_string(deserializer)) {
                return RISA_DESERIALIZATION_ERROR_EOF;
            }

            --count;
        }
    }

    if(!risa_const_buffer_rewind(&deserializer->input, clusterStart)) {
        return RISA_DESERIALIZATION_ERROR_OTHER;
    }

    if(!risa_cluster_deserialize(deserializer, &deserializer->output)) {
        return RISA_DESERIALIZATION_ERROR_EOF;
    }

    return RISA_DESERIALIZATION_OK;
}

static bool risa_cluster_deserialize(RisaClusterDeserializer* deserializer, RisaCluster* cluster) {
    RisaValueArray constants;

    if(!risa_cluster_deserialize_value_array(deserializer, &constants)) {
        return false;
    }

    risa_cluster_init(cluster);

    uint32_t size;

    if(!risa_const_buffer_read_uint32(&deserializer->input, &size)) {
        return false;
    }

    risa_cluster_reserve(cluster, size);

    if(!risa_const_buffer_read(&deserializer->input, cluster->bytecode, size)) {
        return false;
    }

    if(!risa_const_buffer_read(&deserializer->input, (uint8_t*) cluster->indices, sizeof(uint32_t) * size)) {
        return false;
    }

    cluster->size = size;
    cluster->constants = constants;

    return true;
}

static bool risa_cluster_deserialize_string(RisaClusterDeserializer* deserializer) {
    uint32_t length;

    if(!risa_const_buffer_read_uint32(&deserializer->input, &length)) {
        return false;
    }

    uint32_t start;

    if(!risa_const_buffer_skip(&deserializer->input, length, &start)) {
        return false;
    }

    RisaDenseStringPtr str = risa_vm_string_create(deserializer->vm, (const char*) (deserializer->input.data + start), length);

    if(deserializer->strings.size == deserializer->strings.capacity) {
        deserializer->strings.data = RISA_MEM_EXPAND(deserializer->strings.data, &deserializer->strings.capacity, sizeof(uint8_t));
    }

    deserializer->strings.data[deserializer->strings.size++] = str;

    return true;
}

static bool risa_cluster_deserialize_value_array(RisaClusterDeserializer* deserializer, RisaValueArray* array) {
    risa_value_array_init(array);

    uint32_t size;

    if(!risa_const_buffer_read_uint32(&deserializer->input, &size)) {
        return false;
    }

    while(size > 0) {
        RisaValue value;

        if(!risa_cluster_deserialize_value(deserializer, &value)) {
            return false;
        }

        risa_value_array_write(array, value);

        --size;
    }

    return true;
}

static bool risa_cluster_deserialize_value(RisaClusterDeserializer* deserializer, RisaValue* value) {
    uint8_t denseType;
    uint8_t type;

    if(!risa_const_buffer_read_nibbles(&deserializer->input, &denseType, &type)) {
        return false;
    }

    switch((RisaValueType) type) {
        case RISA_VAL_NULL:
            *value = risa_value_from_null();
            return true;
        case RISA_VAL_BOOL: {
            uint8_t val;

            if(!risa_const_buffer_read_uint8(&deserializer->input, &val)) {
                return false;
            }

            *value = risa_value_from_bool((bool) val);
            return true;
        }
        case RISA_VAL_BYTE: {
            uint8_t val;

            if(!risa_const_buffer_read_uint8(&deserializer->input, &val)) {
                return false;
            }

            *value = risa_value_from_byte(val);
            return true;
        }
        case RISA_VAL_INT: {
            int64_t val;

            if(!risa_const_buffer_read_int64(&deserializer->input, &val)) {
                return false;
            }

            *value = risa_value_from_int(val);
            return true;
        }
        case RISA_VAL_FLOAT: {
            double val;

            if(!risa_const_buffer_read_double(&deserializer->input, &val)) {
                return false;
            }

            *value = risa_value_from_float(val);
            return true;
        }
        case RISA_VAL_DENSE: {
            switch((RisaDenseValueType) denseType) {
                case RISA_DVAL_STRING: {
                    uint32_t index;

                    if(!risa_const_buffer_read_uint32(&deserializer->input, &index)) {
                        return false;
                    }

                    if(index >= deserializer->strings.count) {
                        return false;
                    }

                    *value = risa_value_from_dense(deserializer->strings.data[index]);
                    return true;
                }
                case RISA_DVAL_ARRAY: {
                    RisaValueArray array;

                    if(!risa_cluster_deserialize_value_array(deserializer, &array)) {
                        return false;
                    }

                    *value = risa_value_from_dense((RisaDenseValue*) risa_dense_array_create());
                    RISA_AS_ARRAY(*value)->data = array;
                    return true;
                }
                case RISA_DVAL_OBJECT: {
                    uint32_t count;

                    if(!risa_const_buffer_read_uint32(&deserializer->input, &count)) {
                        return false;
                    }

                    *value = risa_value_from_dense((RisaDenseValue*) risa_dense_object_create());

                    while(count > 0) {
                        RisaValue key;

                        if(!risa_cluster_deserialize_value(deserializer, &key)) {
                            return false;
                        }

                        if(!risa_value_is_dense_of_type(key, RISA_DVAL_STRING)) {
                            return false;
                        }

                        RisaValue val;

                        if(!risa_cluster_deserialize_value(deserializer, &val)) {
                            return false;
                        }

                        risa_dense_object_set(RISA_AS_OBJECT(*value), RISA_AS_STRING(key), val);

                        --count;
                    }

                    return true;
                }
                case RISA_DVAL_FUNCTION: {
                    RisaValue name;

                    if(!risa_cluster_deserialize_value(deserializer, &name)) {
                        return false;
                    }

                    if(!risa_value_is_dense_of_type(name, RISA_DVAL_STRING)) {
                        return false;
                    }

                    uint8_t arity;

                    if(!risa_const_buffer_read_uint8(&deserializer->input, &arity)) {
                        return false;
                    }

                    *value = risa_value_from_dense((RisaDenseValue*) risa_dense_function_create());
                    RISA_AS_FUNCTION(*value)->name = RISA_AS_STRING(name);
                    RISA_AS_FUNCTION(*value)->arity = arity;

                    if(!risa_cluster_deserialize(deserializer, &RISA_AS_FUNCTION(*value)->cluster)) {
                        return false;
                    }

                    return true;
                }
            }
        }
        default:
            return false;
    }
}
