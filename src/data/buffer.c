#include "buffer.h"

#include "../mem/mem.h"

#include <string.h>

void risa_buffer_init(RisaBuffer* buffer) {
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

void risa_buffer_delete(RisaBuffer* buffer) {
    RISA_MEM_FREE(buffer->data);

    risa_buffer_init(buffer);
}

uint32_t risa_buffer_write(RisaBuffer* buffer, const uint8_t* src, uint32_t amount) {
    return risa_buffer_write_at(buffer, src, amount, buffer->size);
}

uint32_t risa_buffer_write_at(RisaBuffer* buffer, const uint8_t* src, uint32_t amount, uint32_t offset) {
    if(offset > buffer->size) {
        offset = buffer->size;
    }

    uint32_t diff = buffer->size - offset;
    uint32_t extra = diff < amount ? amount - diff : 0;

    while(buffer->capacity < buffer->size + extra) {
        buffer->data = RISA_MEM_EXPAND(buffer->data, &buffer->capacity, sizeof(uint8_t));
    }

    memcpy(buffer->data + offset, src, amount * sizeof(uint8_t));
    buffer->size += extra;

    return offset;
}

uint32_t risa_buffer_write_nibbles(RisaBuffer* buffer, uint8_t left, uint8_t right) {
    return risa_buffer_write_uint8(buffer, ((left << 4) & 0xF0) | (right  & 0x0F));
}

uint32_t risa_buffer_write_uint8(RisaBuffer* buffer, uint8_t value) {
    return risa_buffer_write(buffer, &value, sizeof(uint8_t));
}

uint32_t risa_buffer_write_uint32(RisaBuffer* buffer, uint32_t value) {
    return risa_buffer_write(buffer, (uint8_t*) &value, sizeof(uint32_t));
}

uint32_t risa_buffer_write_uint32_at(RisaBuffer* buffer, uint32_t value, uint32_t offset) {
    return risa_buffer_write_at(buffer, (uint8_t*) &value, sizeof(uint32_t), offset);
}

uint32_t risa_buffer_write_int64(RisaBuffer* buffer, int64_t value) {
    return risa_buffer_write(buffer, (uint8_t*) &value, sizeof(int64_t));
}

uint32_t risa_buffer_write_double(RisaBuffer* buffer, double value) {
    return risa_buffer_write(buffer, (uint8_t*) &value, sizeof(double));
}

void risa_buffer_patch_size(RisaBuffer* buffer, uint32_t offset) {
    risa_buffer_write_uint32_at(buffer, buffer->size - (offset + sizeof(uint32_t)), offset);
}

uint8_t* risa_buffer_release(RisaBuffer* buffer) {
    uint8_t* ptr = buffer->data;

    // The memory must be freed by the user via RISA_MEM_FREE.
    risa_buffer_init(buffer);

    return ptr;
}

void risa_const_buffer_init(RisaConstBuffer* buffer) {
    buffer->data = NULL;
    buffer->size = 0;
}

void risa_const_buffer_delete(RisaConstBuffer* buffer) {
    risa_const_buffer_init(buffer);
}

bool risa_const_buffer_skip(RisaConstBuffer* buffer, uint32_t amount, uint32_t* backup) {
    *backup = buffer->index;

    if(buffer->index + amount > buffer->size) {
        return false;
    }

    buffer->index += amount;

    return true;
}

bool risa_const_buffer_rewind(RisaConstBuffer* buffer, uint32_t index) {
    if(index >= buffer->size) {
        return false;
    }

    buffer->index = index;
    return true;
}

bool risa_const_buffer_read(RisaConstBuffer* buffer, uint8_t* dest, uint32_t amount) {
    // EOF
    if(buffer->size - buffer->index < amount) {
        return false;
    }

    memcpy(dest, buffer->data + buffer->index, sizeof(uint8_t) * amount);
    buffer->index += sizeof(uint8_t) * amount;

    return true;
}

bool risa_const_buffer_read_nibbles(RisaConstBuffer* buffer, uint8_t* left, uint8_t* right) {
    uint8_t byte;

    if(!risa_const_buffer_read_uint8(buffer, &byte)) {
        return false;
    }

    *left  = (byte >> 4);
    *right = (byte & 0x0F);

    return true;
}

bool risa_const_buffer_read_uint8(RisaConstBuffer* buffer, uint8_t* dest) {
    return risa_const_buffer_read(buffer, dest, sizeof(uint8_t));
}

bool risa_const_buffer_read_uint32(RisaConstBuffer* buffer, uint32_t* dest) {
    return risa_const_buffer_read(buffer, (uint8_t*) dest, sizeof(uint32_t));
}

bool risa_const_buffer_read_int64(RisaConstBuffer* buffer, int64_t* dest) {
    return risa_const_buffer_read(buffer, (uint8_t*) dest, sizeof(int64_t));
}

bool risa_const_buffer_read_double(RisaConstBuffer* buffer, double* dest) {
    return risa_const_buffer_read(buffer, (uint8_t*) dest, sizeof(double));
}