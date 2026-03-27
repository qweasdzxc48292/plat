#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stddef.h>
#include <pthread.h>

#include "hub_config.h"
#include "device_hal.h"

typedef int (*hub_mqtt_message_cb_t)(const char *topic, const char *payload,
                                     char *response, size_t response_size, void *user);

typedef struct hub_mqtt_service {
    hub_config_t config;
    hub_device_hal_t *hal;
    hub_mqtt_message_cb_t on_message;
    void *cb_user;
    pthread_t thread;
    volatile int running;
    void *impl;
} hub_mqtt_service_t;

int hub_mqtt_service_start(hub_mqtt_service_t *svc, const hub_config_t *cfg, hub_device_hal_t *hal,
                           hub_mqtt_message_cb_t cb, void *cb_user);
void hub_mqtt_service_stop(hub_mqtt_service_t *svc);

int hub_mqtt_service_publish(hub_mqtt_service_t *svc, const char *topic, const char *payload, int qos);

#endif /* MQTT_SERVICE_H */

