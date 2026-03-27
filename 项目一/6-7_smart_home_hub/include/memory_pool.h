#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

typedef struct hub_mem_block {
    void *data;
    size_t capacity;
    size_t used;
    int width;
    int height;
    int format;
    uint64_t timestamp_ms;
    uint64_t seq;
    int index;
    atomic_int refs;
} hub_mem_block_t;

typedef struct hub_mem_pool {
    hub_mem_block_t *blocks;
    int block_count;
    size_t block_size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int shutdown;
} hub_mem_pool_t;

int hub_mem_pool_init(hub_mem_pool_t *pool, size_t block_size, int block_count);
void hub_mem_pool_shutdown(hub_mem_pool_t *pool);
void hub_mem_pool_destroy(hub_mem_pool_t *pool);

hub_mem_block_t *hub_mem_pool_acquire(hub_mem_pool_t *pool, int wait_ms);
void hub_mem_pool_retain(hub_mem_block_t *block);
void hub_mem_pool_release(hub_mem_pool_t *pool, hub_mem_block_t *block);

#endif /* MEMORY_POOL_H */

