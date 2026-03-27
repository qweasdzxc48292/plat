#include "memory_pool.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int is_block_free(const hub_mem_block_t *block)
{
    return atomic_load(&block->refs) == 0;
}

int hub_mem_pool_init(hub_mem_pool_t *pool, size_t block_size, int block_count)
{
    int i;

    if (!pool || block_size == 0 || block_count <= 0) {
        return -1;
    }

    memset(pool, 0, sizeof(*pool));
    pool->blocks = (hub_mem_block_t *)calloc((size_t)block_count, sizeof(hub_mem_block_t));
    if (!pool->blocks) {
        return -1;
    }

    pool->block_count = block_count;
    pool->block_size = block_size;
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (i = 0; i < block_count; ++i) {
        pool->blocks[i].data = calloc(1, block_size);
        if (!pool->blocks[i].data) {
            hub_mem_pool_destroy(pool);
            return -1;
        }
        pool->blocks[i].capacity = block_size;
        pool->blocks[i].index = i;
        atomic_store(&pool->blocks[i].refs, 0);
    }

    return 0;
}

void hub_mem_pool_shutdown(hub_mem_pool_t *pool)
{
    if (!pool) {
        return;
    }

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
}

void hub_mem_pool_destroy(hub_mem_pool_t *pool)
{
    int i;

    if (!pool) {
        return;
    }

    for (i = 0; i < pool->block_count; ++i) {
        free(pool->blocks[i].data);
        pool->blocks[i].data = NULL;
    }
    free(pool->blocks);
    pool->blocks = NULL;
    pool->block_count = 0;

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
}

hub_mem_block_t *hub_mem_pool_acquire(hub_mem_pool_t *pool, int wait_ms)
{
    int i;
    struct timespec ts;

    if (!pool || !pool->blocks) {
        return NULL;
    }

    pthread_mutex_lock(&pool->lock);
    while (!pool->shutdown) {
        for (i = 0; i < pool->block_count; ++i) {
            if (is_block_free(&pool->blocks[i])) {
                atomic_store(&pool->blocks[i].refs, 1);
                pool->blocks[i].used = 0;
                pthread_mutex_unlock(&pool->lock);
                return &pool->blocks[i];
            }
        }

        if (wait_ms == 0) {
            break;
        } else if (wait_ms < 0) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        } else {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += wait_ms / 1000;
            ts.tv_nsec += (long)(wait_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
            }
            if (pthread_cond_timedwait(&pool->cond, &pool->lock, &ts) == ETIMEDOUT) {
                break;
            }
        }
    }

    pthread_mutex_unlock(&pool->lock);
    return NULL;
}

void hub_mem_pool_retain(hub_mem_block_t *block)
{
    if (!block) {
        return;
    }
    atomic_fetch_add(&block->refs, 1);
}

void hub_mem_pool_release(hub_mem_pool_t *pool, hub_mem_block_t *block)
{
    int refs;

    if (!pool || !block) {
        return;
    }

    refs = atomic_fetch_sub(&block->refs, 1);
    if (refs <= 1) {
        atomic_store(&block->refs, 0);
        pthread_mutex_lock(&pool->lock);
        pthread_cond_signal(&pool->cond);
        pthread_mutex_unlock(&pool->lock);
    }
}

