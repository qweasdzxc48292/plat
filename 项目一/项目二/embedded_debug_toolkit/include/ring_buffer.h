#ifndef EDT_RING_BUFFER_H
#define EDT_RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t mask;
    atomic_size_t write_pos;
    atomic_size_t read_pos;
} edt_ring_buffer_t;

int edt_ring_buffer_init(edt_ring_buffer_t *rb, size_t capacity_bytes);
void edt_ring_buffer_destroy(edt_ring_buffer_t *rb);
void edt_ring_buffer_reset(edt_ring_buffer_t *rb);

size_t edt_ring_buffer_write(edt_ring_buffer_t *rb, const uint8_t *src, size_t len);
size_t edt_ring_buffer_read(edt_ring_buffer_t *rb, uint8_t *dst, size_t len);
size_t edt_ring_buffer_peek(const edt_ring_buffer_t *rb, uint8_t *dst, size_t len);

size_t edt_ring_buffer_available_data(const edt_ring_buffer_t *rb);
size_t edt_ring_buffer_available_space(const edt_ring_buffer_t *rb);

#endif
