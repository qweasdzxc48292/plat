#include "device_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hub_log.h"

static int ensure_open(hub_device_hal_t *hal, hub_device_id_t dev, int nonblock)
{
    hub_device_handle_t *h;
    int flags;
    const char *fallback_paths[4];
    int i;

    if (!hal || dev < 0 || dev >= HUB_DEVICE_COUNT) {
        return -1;
    }

    h = &hal->devices[dev];
    if (h->fd >= 0) {
        return 0;
    }

    flags = O_RDWR;
    if (nonblock) {
        flags |= O_NONBLOCK;
    }

    h->fd = open(h->path, flags);
    if (h->fd < 0) {
        fallback_paths[0] = NULL;
        fallback_paths[1] = NULL;
        fallback_paths[2] = NULL;
        fallback_paths[3] = NULL;

        if (dev == HUB_DEVICE_LED) {
            fallback_paths[0] = "/dev/hub_led";
            fallback_paths[1] = "/dev/100ask_led";
        } else if (dev == HUB_DEVICE_DHT11) {
            fallback_paths[0] = "/dev/mydht11";
        } else if (dev == HUB_DEVICE_CAMERA) {
            fallback_paths[0] = "/dev/video0";
            fallback_paths[1] = "/dev/video1";
        }

        for (i = 0; i < 4 && fallback_paths[i]; ++i) {
            if (strcmp(h->path, fallback_paths[i]) == 0) {
                continue;
            }
            h->fd = open(fallback_paths[i], flags);
            if (h->fd >= 0) {
                snprintf(h->path, sizeof(h->path), "%s", fallback_paths[i]);
                HUB_LOGI("device fallback open success: %s", h->path);
                return 0;
            }
        }

        HUB_LOGW("open %s failed: %s", h->path, strerror(errno));
        return -1;
    }
    return 0;
}

int hub_device_hal_init(hub_device_hal_t *hal, const hub_config_t *cfg)
{
    int i;

    if (!hal || !cfg) {
        return -1;
    }

    memset(hal, 0, sizeof(*hal));
    for (i = 0; i < HUB_DEVICE_COUNT; ++i) {
        hal->devices[i].fd = -1;
        pthread_mutex_init(&hal->devices[i].lock, NULL);
    }

    snprintf(hal->devices[HUB_DEVICE_LED].path, sizeof(hal->devices[HUB_DEVICE_LED].path), "%s", cfg->led_dev);
    snprintf(hal->devices[HUB_DEVICE_DHT11].path, sizeof(hal->devices[HUB_DEVICE_DHT11].path), "%s", cfg->dht11_dev);
    snprintf(hal->devices[HUB_DEVICE_CAMERA].path, sizeof(hal->devices[HUB_DEVICE_CAMERA].path), "%s", cfg->video_dev);

    (void)hub_device_open(hal, HUB_DEVICE_LED);
    (void)hub_device_open(hal, HUB_DEVICE_DHT11);
    return 0;
}

void hub_device_hal_deinit(hub_device_hal_t *hal)
{
    int i;

    if (!hal) {
        return;
    }

    for (i = 0; i < HUB_DEVICE_COUNT; ++i) {
        hub_device_close(hal, (hub_device_id_t)i);
        pthread_mutex_destroy(&hal->devices[i].lock);
    }
}

int hub_device_open(hub_device_hal_t *hal, hub_device_id_t dev)
{
    if (!hal || dev < 0 || dev >= HUB_DEVICE_COUNT) {
        return -1;
    }

    if (dev == HUB_DEVICE_DHT11) {
        return ensure_open(hal, dev, 1);
    }
    return ensure_open(hal, dev, 0);
}

void hub_device_close(hub_device_hal_t *hal, hub_device_id_t dev)
{
    hub_device_handle_t *h;

    if (!hal || dev < 0 || dev >= HUB_DEVICE_COUNT) {
        return;
    }

    h = &hal->devices[dev];
    pthread_mutex_lock(&h->lock);
    if (h->fd >= 0) {
        close(h->fd);
        h->fd = -1;
    }
    pthread_mutex_unlock(&h->lock);
}

ssize_t hub_device_read(hub_device_hal_t *hal, hub_device_id_t dev, void *buf, size_t size)
{
    hub_device_handle_t *h;
    ssize_t n;

    if (!hal || !buf || size == 0) {
        return -1;
    }
    if (hub_device_open(hal, dev) != 0) {
        return -1;
    }

    h = &hal->devices[dev];
    pthread_mutex_lock(&h->lock);
    n = read(h->fd, buf, size);
    pthread_mutex_unlock(&h->lock);
    return n;
}

ssize_t hub_device_write(hub_device_hal_t *hal, hub_device_id_t dev, const void *buf, size_t size)
{
    hub_device_handle_t *h;
    ssize_t n;

    if (!hal || !buf || size == 0) {
        return -1;
    }
    if (hub_device_open(hal, dev) != 0) {
        return -1;
    }

    h = &hal->devices[dev];
    pthread_mutex_lock(&h->lock);
    n = write(h->fd, buf, size);
    pthread_mutex_unlock(&h->lock);
    return n;
}

int hub_device_ioctl(hub_device_hal_t *hal, hub_device_id_t dev, unsigned long cmd, void *arg)
{
    unsigned char led_buf[2];
    unsigned char dht_buf[2];
    ssize_t n;

    switch (cmd) {
    case HUB_IOCTL_LED_SET: {
        int on = arg ? (*(int *)arg) : 0;
        led_buf[0] = 0;
        led_buf[1] = on ? 0 : 1;
        n = hub_device_write(hal, dev, led_buf, sizeof(led_buf));
        return (n == (ssize_t)sizeof(led_buf)) ? 0 : -1;
    }
    case HUB_IOCTL_LED_GET:
        if (!arg) {
            return -1;
        }
        led_buf[0] = 0;
        n = hub_device_read(hal, dev, led_buf, sizeof(led_buf));
        if (n != (ssize_t)sizeof(led_buf)) {
            return -1;
        }
        *(int *)arg = (led_buf[1] == 0) ? 1 : 0;
        return 0;
    case HUB_IOCTL_DHT11_READ:
        if (!arg) {
            return -1;
        }
        n = hub_device_read(hal, dev, dht_buf, sizeof(dht_buf));
        if (n != (ssize_t)sizeof(dht_buf)) {
            return -1;
        }
        ((int *)arg)[0] = (int)dht_buf[0];
        ((int *)arg)[1] = (int)dht_buf[1];
        return 0;
    default:
        return -1;
    }
}

int hub_led_set(hub_device_hal_t *hal, int on)
{
    return hub_device_ioctl(hal, HUB_DEVICE_LED, HUB_IOCTL_LED_SET, &on);
}

int hub_led_get(hub_device_hal_t *hal, int *on)
{
    return hub_device_ioctl(hal, HUB_DEVICE_LED, HUB_IOCTL_LED_GET, on);
}

int hub_dht11_read(hub_device_hal_t *hal, int *humi, int *temp)
{
    int values[2];

    if (!humi || !temp) {
        return -1;
    }
    if (hub_device_ioctl(hal, HUB_DEVICE_DHT11, HUB_IOCTL_DHT11_READ, values) != 0) {
        return -1;
    }

    *humi = values[0];
    *temp = values[1];
    return 0;
}
