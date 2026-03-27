#ifndef VIDEO_PIPELINE_H
#define VIDEO_PIPELINE_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "hub_config.h"
#include "memory_pool.h"

#define HUB_PIXEL_FMT_RGB565 1

typedef struct hub_video_frame {
    const uint8_t *data;
    size_t size;
    int width;
    int height;
    int format;
    uint64_t seq;
    uint64_t timestamp_ms;
} hub_video_frame_t;

typedef void (*hub_video_frame_cb_t)(const hub_video_frame_t *frame, void *user);

typedef struct hub_video_pipeline {
    hub_config_t config;
    hub_mem_pool_t *pool;
    hub_video_frame_cb_t on_frame;
    void *cb_user;
    pthread_t capture_thread;
    pthread_t render_thread;
    volatile int running;
    void *priv;
} hub_video_pipeline_t;

int hub_video_pipeline_start(hub_video_pipeline_t *pl, const hub_config_t *cfg, hub_mem_pool_t *pool,
                             hub_video_frame_cb_t cb, void *cb_user);
void hub_video_pipeline_stop(hub_video_pipeline_t *pl);

#endif /* VIDEO_PIPELINE_H */

