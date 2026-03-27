#ifndef HUB_CONFIG_H
#define HUB_CONFIG_H

#include <stddef.h>

typedef struct hub_config {
    char mqtt_host[128];
    int mqtt_port;
    char mqtt_client_id[128];
    char mqtt_username[128];
    char mqtt_password[128];
    char mqtt_topic_up[128];
    char mqtt_topic_down[128];
    char mqtt_topic_resp[128];
    int mqtt_qos_report;
    int mqtt_qos_control;
    int mqtt_qos_response;
    int mqtt_keepalive_sec;
    int mqtt_reconnect_sec;
    int report_interval_sec;

    char led_dev[64];
    char dht11_dev[64];
    char video_dev[64];

    int video_width;
    int video_height;
    int video_fps;

    int pool_block_count;
    int pool_block_size;

    char media_default_file[256];
} hub_config_t;

void hub_config_set_defaults(hub_config_t *cfg);
int hub_config_load(const char *path, hub_config_t *cfg);

#endif /* HUB_CONFIG_H */
