#include "hub_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hub_log.h"

static const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

static int json_find_key(const char *json, const char *key, const char **out_val)
{
    char pattern[96];
    const char *p;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return -1;
    }

    p = strchr(p + strlen(pattern), ':');
    if (!p) {
        return -1;
    }

    p = skip_ws(p + 1);
    *out_val = p;
    return 0;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    const char *p;
    size_t len;

    if (json_find_key(json, key, &p) != 0 || *p != '"') {
        return -1;
    }

    ++p;
    len = 0;
    while (p[len] && p[len] != '"' && len + 1 < out_size) {
        out[len] = p[len];
        ++len;
    }

    if (p[len] != '"') {
        return -1;
    }

    out[len] = '\0';
    return 0;
}

static int json_get_int(const char *json, const char *key, int *out)
{
    const char *p;
    char *endp;
    long value;

    if (json_find_key(json, key, &p) != 0) {
        return -1;
    }

    value = strtol(p, &endp, 10);
    if (p == endp) {
        return -1;
    }

    *out = (int)value;
    return 0;
}

void hub_config_set_defaults(hub_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    snprintf(cfg->mqtt_host, sizeof(cfg->mqtt_host), "127.0.0.1");
    cfg->mqtt_port = 1883;
    snprintf(cfg->mqtt_client_id, sizeof(cfg->mqtt_client_id), "smart_home_hub");
    snprintf(cfg->mqtt_topic_up, sizeof(cfg->mqtt_topic_up), "/iot/up");
    snprintf(cfg->mqtt_topic_down, sizeof(cfg->mqtt_topic_down), "/iot/down");
    snprintf(cfg->mqtt_topic_resp, sizeof(cfg->mqtt_topic_resp), "/iot/resp");
    cfg->mqtt_qos_report = 0;
    cfg->mqtt_qos_control = 1;
    cfg->mqtt_qos_response = 1;
    cfg->mqtt_keepalive_sec = 20;
    cfg->mqtt_reconnect_sec = 5;
    cfg->report_interval_sec = 5;

    snprintf(cfg->led_dev, sizeof(cfg->led_dev), "/dev/hub_led");
    snprintf(cfg->dht11_dev, sizeof(cfg->dht11_dev), "/dev/mydht11");
    snprintf(cfg->video_dev, sizeof(cfg->video_dev), "/dev/video0");
    cfg->video_width = 640;
    cfg->video_height = 480;
    cfg->video_fps = 30;

    cfg->pool_block_count = 4;
    cfg->pool_block_size = 1920 * 1080 * 2;
    snprintf(cfg->media_default_file, sizeof(cfg->media_default_file), "/root/media/test.wav");
}

int hub_config_load(const char *path, hub_config_t *cfg)
{
    FILE *fp;
    long sz;
    char *buf;

    if (!cfg) {
        return -1;
    }

    hub_config_set_defaults(cfg);

    fp = fopen(path, "rb");
    if (!fp) {
        HUB_LOGW("config file %s not found, use defaults", path ? path : "(null)");
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz <= 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return -1;
    }
    buf[sz] = '\0';
    fclose(fp);

    (void)json_get_string(buf, "mqtt_host", cfg->mqtt_host, sizeof(cfg->mqtt_host));
    (void)json_get_int(buf, "mqtt_port", &cfg->mqtt_port);
    (void)json_get_string(buf, "mqtt_client_id", cfg->mqtt_client_id, sizeof(cfg->mqtt_client_id));
    (void)json_get_string(buf, "mqtt_username", cfg->mqtt_username, sizeof(cfg->mqtt_username));
    (void)json_get_string(buf, "mqtt_password", cfg->mqtt_password, sizeof(cfg->mqtt_password));
    (void)json_get_string(buf, "mqtt_topic_up", cfg->mqtt_topic_up, sizeof(cfg->mqtt_topic_up));
    (void)json_get_string(buf, "mqtt_topic_down", cfg->mqtt_topic_down, sizeof(cfg->mqtt_topic_down));
    (void)json_get_string(buf, "mqtt_topic_resp", cfg->mqtt_topic_resp, sizeof(cfg->mqtt_topic_resp));
    (void)json_get_int(buf, "mqtt_qos_report", &cfg->mqtt_qos_report);
    (void)json_get_int(buf, "mqtt_qos_control", &cfg->mqtt_qos_control);
    (void)json_get_int(buf, "mqtt_qos_response", &cfg->mqtt_qos_response);
    (void)json_get_int(buf, "mqtt_keepalive_sec", &cfg->mqtt_keepalive_sec);
    (void)json_get_int(buf, "mqtt_reconnect_sec", &cfg->mqtt_reconnect_sec);
    (void)json_get_int(buf, "report_interval_sec", &cfg->report_interval_sec);

    (void)json_get_string(buf, "led_dev", cfg->led_dev, sizeof(cfg->led_dev));
    (void)json_get_string(buf, "dht11_dev", cfg->dht11_dev, sizeof(cfg->dht11_dev));
    (void)json_get_string(buf, "video_dev", cfg->video_dev, sizeof(cfg->video_dev));
    (void)json_get_int(buf, "video_width", &cfg->video_width);
    (void)json_get_int(buf, "video_height", &cfg->video_height);
    (void)json_get_int(buf, "video_fps", &cfg->video_fps);

    (void)json_get_int(buf, "pool_block_count", &cfg->pool_block_count);
    (void)json_get_int(buf, "pool_block_size", &cfg->pool_block_size);
    (void)json_get_string(buf, "media_default_file", cfg->media_default_file, sizeof(cfg->media_default_file));

    if (cfg->pool_block_size <= 0) {
        cfg->pool_block_size = 1920 * 1080 * 2;
    }
    if (cfg->pool_block_count <= 0) {
        cfg->pool_block_count = 4;
    }
    if (cfg->mqtt_qos_report < 0 || cfg->mqtt_qos_report > 2) {
        cfg->mqtt_qos_report = 0;
    }
    if (cfg->mqtt_qos_control < 0 || cfg->mqtt_qos_control > 2) {
        cfg->mqtt_qos_control = 1;
    }
    if (cfg->mqtt_qos_response < 0 || cfg->mqtt_qos_response > 2) {
        cfg->mqtt_qos_response = 1;
    }

    free(buf);
    return 0;
}
