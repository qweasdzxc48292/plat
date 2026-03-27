#ifndef DEVICE_HAL_H
#define DEVICE_HAL_H

#include <stddef.h>
#include <sys/types.h>
#include <pthread.h>

#include "hub_config.h"

typedef enum {
    HUB_DEVICE_LED = 0,
    HUB_DEVICE_DHT11,
    HUB_DEVICE_CAMERA,
    HUB_DEVICE_COUNT
} hub_device_id_t;

typedef struct hub_device_handle {
    int fd;
    char path[64];
    pthread_mutex_t lock;
} hub_device_handle_t;

typedef struct hub_device_hal {
    hub_device_handle_t devices[HUB_DEVICE_COUNT];
} hub_device_hal_t;

#define HUB_IOCTL_LED_SET    0x1001
#define HUB_IOCTL_LED_GET    0x1002
#define HUB_IOCTL_DHT11_READ 0x2001

int hub_device_hal_init(hub_device_hal_t *hal, const hub_config_t *cfg);
void hub_device_hal_deinit(hub_device_hal_t *hal);

int hub_device_open(hub_device_hal_t *hal, hub_device_id_t dev);
void hub_device_close(hub_device_hal_t *hal, hub_device_id_t dev);

ssize_t hub_device_read(hub_device_hal_t *hal, hub_device_id_t dev, void *buf, size_t size);
ssize_t hub_device_write(hub_device_hal_t *hal, hub_device_id_t dev, const void *buf, size_t size);
int hub_device_ioctl(hub_device_hal_t *hal, hub_device_id_t dev, unsigned long cmd, void *arg);

int hub_led_set(hub_device_hal_t *hal, int on);
int hub_led_get(hub_device_hal_t *hal, int *on);
int hub_dht11_read(hub_device_hal_t *hal, int *humi, int *temp);

#endif /* DEVICE_HAL_H */

