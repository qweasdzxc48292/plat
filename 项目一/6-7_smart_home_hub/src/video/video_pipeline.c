#include "video_pipeline.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#endif

#include "hub_log.h"

#define HUB_VIDEO_QUEUE_DEPTH 8
#define HUB_V4L2_BUF_COUNT 4
#define HUB_VIDEO_STATS_FILE "/tmp/hub_video_stats.json"

#ifdef __linux__
static void close_v4l2(hub_video_pipeline_t *pl);
#endif

typedef struct hub_video_priv {
    pthread_mutex_t q_lock;
    pthread_cond_t q_cond;
    hub_mem_block_t *queue[HUB_VIDEO_QUEUE_DEPTH];
    int q_head;
    int q_tail;
    int q_count;
    uint64_t seq;
    int width;
    int height;
    uint64_t stat_window_start_ms;
    uint64_t stat_rendered_frames;
    uint64_t stat_captured_frames;
    uint64_t stat_drop_pool;
    uint64_t stat_drop_queue;

    int lut_rv[256];
    int lut_gu[256];
    int lut_gv[256];
    int lut_bu[256];

#ifdef __linux__
    int fd;
    struct {
        void *start;
        size_t len;
    } bufs[HUB_V4L2_BUF_COUNT];
    int buf_count;
#endif
} hub_video_priv_t;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int clip_0_255(int v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return v;
}

static void init_yuv_lut(hub_video_priv_t *priv)
{
    int i;
    for (i = 0; i < 256; ++i) {
        priv->lut_rv[i] = (i - 128) * 1402 / 1000;
        priv->lut_gu[i] = (128 - i) * 344 / 1000;
        priv->lut_gv[i] = (128 - i) * 714 / 1000;
        priv->lut_bu[i] = (i - 128) * 1772 / 1000;
    }
}

static void yuyv_to_rgb565(hub_video_priv_t *priv, const unsigned char *in, size_t in_len,
                           unsigned char *out, size_t out_cap, int width, int height)
{
    size_t need = (size_t)width * (size_t)height * 2U;
    size_t pos = 0;
    size_t i;

    if (in_len < (size_t)width * (size_t)height * 2U || out_cap < need) {
        return;
    }

    for (i = 0; i + 3 < in_len && pos + 3 < need; i += 4) {
        int y0 = in[i + 0];
        int u = in[i + 1];
        int y1 = in[i + 2];
        int v = in[i + 3];
        int r, g, b;
        unsigned short rgb565;

        r = clip_0_255(y0 + priv->lut_rv[v]);
        g = clip_0_255(y0 + priv->lut_gu[u] + priv->lut_gv[v]);
        b = clip_0_255(y0 + priv->lut_bu[u]);
        rgb565 = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        out[pos++] = (unsigned char)(rgb565 & 0xFF);
        out[pos++] = (unsigned char)((rgb565 >> 8) & 0xFF);

        r = clip_0_255(y1 + priv->lut_rv[v]);
        g = clip_0_255(y1 + priv->lut_gu[u] + priv->lut_gv[v]);
        b = clip_0_255(y1 + priv->lut_bu[u]);
        rgb565 = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        out[pos++] = (unsigned char)(rgb565 & 0xFF);
        out[pos++] = (unsigned char)((rgb565 >> 8) & 0xFF);
    }
}

static void queue_push(hub_video_pipeline_t *pl, hub_mem_block_t *block)
{
    hub_video_priv_t *priv = (hub_video_priv_t *)pl->priv;
    hub_mem_block_t *drop = NULL;

    pthread_mutex_lock(&priv->q_lock);
    if (priv->q_count >= HUB_VIDEO_QUEUE_DEPTH) {
        drop = priv->queue[priv->q_head];
        priv->q_head = (priv->q_head + 1) % HUB_VIDEO_QUEUE_DEPTH;
        priv->q_count--;
    }

    priv->queue[priv->q_tail] = block;
    priv->q_tail = (priv->q_tail + 1) % HUB_VIDEO_QUEUE_DEPTH;
    priv->q_count++;
    pthread_cond_signal(&priv->q_cond);
    pthread_mutex_unlock(&priv->q_lock);

    if (drop) {
        priv->stat_drop_queue++;
        hub_mem_pool_release(pl->pool, drop);
    }
}

static hub_mem_block_t *queue_pop(hub_video_pipeline_t *pl, int wait_ms)
{
    hub_video_priv_t *priv = (hub_video_priv_t *)pl->priv;
    hub_mem_block_t *block = NULL;
    struct timespec ts;

    pthread_mutex_lock(&priv->q_lock);
    while (pl->running && priv->q_count == 0) {
        if (wait_ms < 0) {
            pthread_cond_wait(&priv->q_cond, &priv->q_lock);
        } else {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += wait_ms / 1000;
            ts.tv_nsec += (long)(wait_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
            }
            if (pthread_cond_timedwait(&priv->q_cond, &priv->q_lock, &ts) == ETIMEDOUT) {
                break;
            }
        }
    }

    if (priv->q_count > 0) {
        block = priv->queue[priv->q_head];
        priv->q_head = (priv->q_head + 1) % HUB_VIDEO_QUEUE_DEPTH;
        priv->q_count--;
    }
    pthread_mutex_unlock(&priv->q_lock);
    return block;
}

static void default_frame_sink(const hub_video_frame_t *frame, void *user)
{
    hub_video_priv_t *priv = (hub_video_priv_t *)user;
    uint64_t now = now_ms();
    FILE *fp;
    unsigned long long fps;

    if (!priv) {
        return;
    }

    if (!priv->stat_window_start_ms) {
        priv->stat_window_start_ms = now;
    }
    priv->stat_rendered_frames++;
    if (now - priv->stat_window_start_ms >= 1000) {
        fps = (unsigned long long)priv->stat_rendered_frames;
        HUB_LOGI("video fps=%llu drop_pool=%llu drop_queue=%llu size=%dx%d seq=%llu",
                 fps,
                 (unsigned long long)priv->stat_drop_pool,
                 (unsigned long long)priv->stat_drop_queue,
                 frame->width, frame->height,
                 (unsigned long long)frame->seq);
        fp = fopen(HUB_VIDEO_STATS_FILE, "wb");
        if (fp) {
            fprintf(fp,
                    "{\"fps\":%llu,\"width\":%d,\"height\":%d,\"seq\":%llu,\"timestamp_ms\":%llu,"
                    "\"captured\":%llu,\"drop_pool\":%llu,\"drop_queue\":%llu}\n",
                    fps, frame->width, frame->height,
                    (unsigned long long)frame->seq,
                    (unsigned long long)frame->timestamp_ms,
                    (unsigned long long)priv->stat_captured_frames,
                    (unsigned long long)priv->stat_drop_pool,
                    (unsigned long long)priv->stat_drop_queue);
            fclose(fp);
        }
        priv->stat_window_start_ms = now;
        priv->stat_rendered_frames = 0;
    }
}

#ifdef __linux__
static int open_v4l2(hub_video_pipeline_t *pl)
{
    hub_video_priv_t *priv = (hub_video_priv_t *)pl->priv;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    int i;

    priv->fd = open(pl->config.video_dev, O_RDWR | O_NONBLOCK);
    if (priv->fd < 0) {
        HUB_LOGW("open camera %s failed: %s", pl->config.video_dev, strerror(errno));
        return -1;
    }

    if (ioctl(priv->fd, VIDIOC_QUERYCAP, &cap) != 0) {
        HUB_LOGE("VIDIOC_QUERYCAP failed: %s", strerror(errno));
        goto err_out;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = pl->config.video_width;
    fmt.fmt.pix.height = pl->config.video_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(priv->fd, VIDIOC_S_FMT, &fmt) != 0) {
        HUB_LOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        goto err_out;
    }
    priv->width = (int)fmt.fmt.pix.width;
    priv->height = (int)fmt.fmt.pix.height;

    memset(&req, 0, sizeof(req));
    req.count = HUB_V4L2_BUF_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(priv->fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
        HUB_LOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        goto err_out;
    }

    priv->buf_count = req.count > HUB_V4L2_BUF_COUNT ? HUB_V4L2_BUF_COUNT : (int)req.count;
    for (i = 0; i < priv->buf_count; ++i) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = (unsigned int)i;
        if (ioctl(priv->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            HUB_LOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            goto err_out;
        }
        priv->bufs[i].len = buf.length;
        priv->bufs[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, buf.m.offset);
        if (priv->bufs[i].start == MAP_FAILED) {
            HUB_LOGE("mmap failed: %s", strerror(errno));
            goto err_out;
        }
        if (ioctl(priv->fd, VIDIOC_QBUF, &buf) != 0) {
            HUB_LOGE("VIDIOC_QBUF failed: %s", strerror(errno));
            goto err_out;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(priv->fd, VIDIOC_STREAMON, &type) != 0) {
        HUB_LOGE("VIDIOC_STREAMON failed: %s", strerror(errno));
        goto err_out;
    }

    HUB_LOGI("video opened: %s %dx%d", pl->config.video_dev, priv->width, priv->height);
    return 0;

err_out:
    close_v4l2(pl);
    return -1;
}

static void close_v4l2(hub_video_pipeline_t *pl)
{
    hub_video_priv_t *priv = (hub_video_priv_t *)pl->priv;
    enum v4l2_buf_type type;
    int i;

    if (priv->fd < 0) {
        return;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    (void)ioctl(priv->fd, VIDIOC_STREAMOFF, &type);

    for (i = 0; i < priv->buf_count; ++i) {
        if (priv->bufs[i].start && priv->bufs[i].start != MAP_FAILED) {
            munmap(priv->bufs[i].start, priv->bufs[i].len);
            priv->bufs[i].start = NULL;
        }
    }
    close(priv->fd);
    priv->fd = -1;
}
#endif

static void *capture_thread_main(void *arg)
{
    hub_video_pipeline_t *pl = (hub_video_pipeline_t *)arg;
    hub_video_priv_t *priv = (hub_video_priv_t *)pl->priv;

#ifdef __linux__
    if (priv->fd >= 0) {
        struct pollfd pfd;
        struct v4l2_buffer vbuf;

        pfd.fd = priv->fd;
        pfd.events = POLLIN;

        while (pl->running) {
            int pr = poll(&pfd, 1, 1000);
            if (pr <= 0) {
                continue;
            }

            memset(&vbuf, 0, sizeof(vbuf));
            vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vbuf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(priv->fd, VIDIOC_DQBUF, &vbuf) != 0) {
                continue;
            }

            if (vbuf.index < (unsigned int)priv->buf_count && vbuf.bytesused > 0) {
                priv->stat_captured_frames++;
                hub_mem_block_t *blk = hub_mem_pool_acquire(pl->pool, 20);
                if (blk) {
                    yuyv_to_rgb565(priv, (const unsigned char *)priv->bufs[vbuf.index].start, vbuf.bytesused,
                                   (unsigned char *)blk->data, blk->capacity, priv->width, priv->height);
                    blk->used = (size_t)priv->width * (size_t)priv->height * 2U;
                    blk->width = priv->width;
                    blk->height = priv->height;
                    blk->format = HUB_PIXEL_FMT_RGB565;
                    blk->seq = ++priv->seq;
                    blk->timestamp_ms = now_ms();
                    queue_push(pl, blk);
                } else {
                    priv->stat_drop_pool++;
                }
            }

            (void)ioctl(priv->fd, VIDIOC_QBUF, &vbuf);
        }
        return NULL;
    }
#endif

    while (pl->running) {
        hub_mem_block_t *blk = hub_mem_pool_acquire(pl->pool, 50);
        int fps = pl->config.video_fps > 0 ? pl->config.video_fps : 25;
        int interval_us = 1000000 / fps;

        if (blk) {
            priv->stat_captured_frames++;
            memset(blk->data, 0, blk->capacity);
            blk->used = blk->capacity;
            blk->width = priv->width;
            blk->height = priv->height;
            blk->format = HUB_PIXEL_FMT_RGB565;
            blk->seq = ++priv->seq;
            blk->timestamp_ms = now_ms();
            queue_push(pl, blk);
        } else {
            priv->stat_drop_pool++;
        }
        usleep((useconds_t)interval_us);
    }
    return NULL;
}

static void *render_thread_main(void *arg)
{
    hub_video_pipeline_t *pl = (hub_video_pipeline_t *)arg;

    while (pl->running) {
        hub_mem_block_t *blk = queue_pop(pl, 200);
        hub_video_frame_t frame;

        if (!blk) {
            continue;
        }

        frame.data = (const uint8_t *)blk->data;
        frame.size = blk->used;
        frame.width = blk->width;
        frame.height = blk->height;
        frame.format = blk->format;
        frame.seq = blk->seq;
        frame.timestamp_ms = blk->timestamp_ms;

        if (pl->on_frame) {
            pl->on_frame(&frame, pl->cb_user);
        }

        hub_mem_pool_release(pl->pool, blk);
    }
    return NULL;
}

int hub_video_pipeline_start(hub_video_pipeline_t *pl, const hub_config_t *cfg, hub_mem_pool_t *pool,
                             hub_video_frame_cb_t cb, void *cb_user)
{
    hub_video_priv_t *priv;

    if (!pl || !cfg || !pool) {
        return -1;
    }

    memset(pl, 0, sizeof(*pl));
    pl->config = *cfg;
    pl->pool = pool;
    pl->on_frame = cb ? cb : default_frame_sink;
    pl->cb_user = cb_user;

    priv = (hub_video_priv_t *)calloc(1, sizeof(*priv));
    if (!priv) {
        return -1;
    }
    pl->priv = priv;
    if (!cb) {
        pl->cb_user = priv;
    }
    priv->width = cfg->video_width;
    priv->height = cfg->video_height;
    init_yuv_lut(priv);
    pthread_mutex_init(&priv->q_lock, NULL);
    pthread_cond_init(&priv->q_cond, NULL);

#ifdef __linux__
    priv->fd = -1;
    if (open_v4l2(pl) != 0) {
        HUB_LOGW("v4l2 not ready, fallback to synthetic frames");
    }
#endif

    pl->running = 1;
    if (pthread_create(&pl->capture_thread, NULL, capture_thread_main, pl) != 0) {
        pl->running = 0;
        hub_video_pipeline_stop(pl);
        return -1;
    }
    if (pthread_create(&pl->render_thread, NULL, render_thread_main, pl) != 0) {
        pl->running = 0;
        hub_video_pipeline_stop(pl);
        return -1;
    }

    HUB_LOGI("video pipeline started");
    return 0;
}

void hub_video_pipeline_stop(hub_video_pipeline_t *pl)
{
    hub_video_priv_t *priv;
    int i;

    if (!pl || !pl->priv) {
        return;
    }

    priv = (hub_video_priv_t *)pl->priv;
    pl->running = 0;

    pthread_mutex_lock(&priv->q_lock);
    pthread_cond_broadcast(&priv->q_cond);
    pthread_mutex_unlock(&priv->q_lock);

    if (pl->capture_thread) {
        pthread_join(pl->capture_thread, NULL);
        pl->capture_thread = 0;
    }
    if (pl->render_thread) {
        pthread_join(pl->render_thread, NULL);
        pl->render_thread = 0;
    }

    for (i = 0; i < HUB_VIDEO_QUEUE_DEPTH; ++i) {
        if (priv->queue[i]) {
            hub_mem_pool_release(pl->pool, priv->queue[i]);
            priv->queue[i] = NULL;
        }
    }

#ifdef __linux__
    close_v4l2(pl);
#endif
    pthread_mutex_destroy(&priv->q_lock);
    pthread_cond_destroy(&priv->q_cond);

    free(priv);
    pl->priv = NULL;
    HUB_LOGI("video pipeline stopped");
}
