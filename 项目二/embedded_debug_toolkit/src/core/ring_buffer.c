#include "ring_buffer.h"

#include <stdlib.h>
#include <string.h>

static int edt_is_power_of_two(size_t v)
{
    return v != 0U && (v & (v - 1U)) == 0U;
}

static size_t edt_round_up_pow2(size_t v)
{
    size_t p = 1U;
    while (p < v) {
        p <<= 1U;
    }
    return p;
}

int edt_ring_buffer_init(edt_ring_buffer_t *rb, size_t capacity_bytes)
{
    if (!rb || capacity_bytes < 2U) {
        return -1;
    }
    if (!edt_is_power_of_two(capacity_bytes)) {
        capacity_bytes = edt_round_up_pow2(capacity_bytes);
    }
    rb->data = (uint8_t *)calloc(capacity_bytes, sizeof(uint8_t));
    if (!rb->data) {
        return -1;
    }
    rb->capacity = capacity_bytes;
    rb->mask = capacity_bytes - 1U;
    atomic_store(&rb->write_pos, 0U);
    atomic_store(&rb->read_pos, 0U);
    return 0;
}

void edt_ring_buffer_destroy(edt_ring_buffer_t *rb)
{
    if (!rb) {
        return;
    }
    free(rb->data);
    rb->data = NULL;
    rb->capacity = 0U;
    rb->mask = 0U;
    atomic_store(&rb->write_pos, 0U);
    atomic_store(&rb->read_pos, 0U);
}

void edt_ring_buffer_reset(edt_ring_buffer_t *rb)
{
    if (!rb) {
        return;
    }
    atomic_store(&rb->write_pos, 0U);
    atomic_store(&rb->read_pos, 0U);
}

size_t edt_ring_buffer_available_data(const edt_ring_buffer_t *rb)
{
    if (!rb || !rb->data) {
        return 0U;
    }
    const size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    const size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    return write_pos - read_pos;
}

size_t edt_ring_buffer_available_space(const edt_ring_buffer_t *rb)
{
    if (!rb || !rb->data) {
        return 0U;
    }
    return rb->capacity - edt_ring_buffer_available_data(rb);
}

size_t edt_ring_buffer_write(edt_ring_buffer_t *rb, const uint8_t *src, size_t len)
{
    size_t write_pos;
    size_t read_pos;
    size_t free_space;
    size_t to_write;
    size_t first_chunk;

    if (!rb || !rb->data || !src || len == 0U) {
        return 0U;
    }

    write_pos = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    free_space = rb->capacity - (write_pos - read_pos);
    to_write = (len < free_space) ? len : free_space;
    if (to_write == 0U) {
        return 0U;
    }

    first_chunk = rb->capacity - (write_pos & rb->mask);
    if (first_chunk > to_write) {
        first_chunk = to_write;
    }

    memcpy(rb->data + (write_pos & rb->mask), src, first_chunk);
    if (first_chunk < to_write) {
        memcpy(rb->data, src + first_chunk, to_write - first_chunk);
    }

    atomic_store_explicit(&rb->write_pos, write_pos + to_write, memory_order_release);
    return to_write;
}

size_t edt_ring_buffer_read(edt_ring_buffer_t *rb, uint8_t *dst, size_t len)
{
    size_t write_pos;
    size_t read_pos;
    size_t available;
    size_t to_read;
    size_t first_chunk;

    if (!rb || !rb->data || !dst || len == 0U) {
        return 0U;
    }

    write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    available = write_pos - read_pos;
    to_read = (len < available) ? len : available;
    if (to_read == 0U) {
        return 0U;
    }

    first_chunk = rb->capacity - (read_pos & rb->mask);
    if (first_chunk > to_read) {
        first_chunk = to_read;
    }

    memcpy(dst, rb->data + (read_pos & rb->mask), first_chunk);
    if (first_chunk < to_read) {
        memcpy(dst + first_chunk, rb->data, to_read - first_chunk);
    }

    atomic_store_explicit(&rb->read_pos, read_pos + to_read, memory_order_release);
    return to_read;
}

size_t edt_ring_buffer_peek(const edt_ring_buffer_t *rb, uint8_t *dst, size_t len)
{
    size_t write_pos;
    size_t read_pos;
    size_t available;
    size_t to_read;
    size_t first_chunk;

    if (!rb || !rb->data || !dst || len == 0U) {
        return 0U;
    }

    write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    available = write_pos - read_pos;
    to_read = (len < available) ? len : available;
    if (to_read == 0U) {
        return 0U;
    }

    first_chunk = rb->capacity - (read_pos & rb->mask);
    if (first_chunk > to_read) {
        first_chunk = to_read;
    }

    memcpy(dst, rb->data + (read_pos & rb->mask), first_chunk);
    if (first_chunk < to_read) {
        memcpy(dst + first_chunk, rb->data, to_read - first_chunk);
    }

    return to_read;
}
