#ifndef RISA_DATA_BUFFER_H_GUARD
#define RISA_DATA_BUFFER_H_GUARD

#include "../api.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
} RisaBuffer;

typedef struct {
    const uint8_t* data;
    uint32_t size;
    uint32_t index;
} RisaConstBuffer;

RISA_API void     risa_buffer_init            (RisaBuffer* buffer);
RISA_API void     risa_buffer_delete          (RisaBuffer* buffer);
RISA_API uint32_t risa_buffer_write           (RisaBuffer* buffer, const uint8_t* src, uint32_t amount);
RISA_API uint32_t risa_buffer_write_at        (RisaBuffer* buffer, const uint8_t* src, uint32_t amount, uint32_t offset);
RISA_API uint32_t risa_buffer_write_nibbles   (RisaBuffer* buffer, uint8_t left, uint8_t right);
RISA_API uint32_t risa_buffer_write_uint8     (RisaBuffer* buffer, uint8_t value);
RISA_API uint32_t risa_buffer_write_uint32    (RisaBuffer* buffer, uint32_t value);
RISA_API uint32_t risa_buffer_write_uint32_at (RisaBuffer* buffer, uint32_t value, uint32_t offset);
RISA_API uint32_t risa_buffer_write_int64     (RisaBuffer* buffer, int64_t value);
RISA_API uint32_t risa_buffer_write_double    (RisaBuffer* buffer, double value);
RISA_API void     risa_buffer_patch_size      (RisaBuffer* buffer, uint32_t offset);
RISA_API uint8_t* risa_buffer_release         (RisaBuffer* buffer);

RISA_API void     risa_const_buffer_init        (RisaConstBuffer* buffer);
RISA_API void     risa_const_buffer_delete      (RisaConstBuffer* buffer);
RISA_API bool     risa_const_buffer_skip        (RisaConstBuffer* buffer, uint32_t amount, uint32_t* backup);
RISA_API bool     risa_const_buffer_rewind      (RisaConstBuffer* buffer, uint32_t index);
RISA_API bool     risa_const_buffer_read        (RisaConstBuffer* buffer, uint8_t* dest, uint32_t amount);
RISA_API bool     risa_const_buffer_read_nibbles(RisaConstBuffer* buffer, uint8_t* left, uint8_t* right);
RISA_API bool     risa_const_buffer_read_uint8  (RisaConstBuffer* buffer, uint8_t* dest);
RISA_API bool     risa_const_buffer_read_uint32 (RisaConstBuffer* buffer, uint32_t* dest);
RISA_API bool     risa_const_buffer_read_int64  (RisaConstBuffer* buffer, int64_t* dest);
RISA_API bool     risa_const_buffer_read_double (RisaConstBuffer* buffer, double* dest);

#endif
