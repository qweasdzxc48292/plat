#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "device_hal.h"
#include "hub_config.h"
#include "hub_log.h"
#include "json_rpc_dispatch.h"
#include "memory_pool.h"
#include "media_player.h"
#include "mqtt_service.h"
#include "video_pipeline.h"

typedef struct hub_runtime {
    hub_config_t cfg;
    hub_device_hal_t hal;
    hub_mem_pool_t pool;
    hub_video_pipeline_t video;
    hub_media_player_t media;
    hub_mqtt_service_t mqtt;
    hub_rpc_context_t rpc;
} hub_runtime_t;

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int on_mqtt_message(const char *topic, const char *payload, char *response, size_t response_size, void *user)
{
    hub_runtime_t *rt = (hub_runtime_t *)user;

    HUB_LOGI("mqtt recv topic=%s payload=%s", topic ? topic : "", payload ? payload : "");
    if (!rt || !payload || !response) {
        return -1;
    }
    return hub_json_rpc_dispatch(&rt->rpc, payload, response, response_size);
}

int main(int argc, char **argv)
{
    const char *cfg_path = "config/hub_config.json";
    hub_runtime_t rt;

    if (argc > 1 && argv[1] && argv[1][0]) {
        cfg_path = argv[1];
    }

    memset(&rt, 0, sizeof(rt));
    hub_log_set_level(HUB_LOG_INFO);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (hub_config_load(cfg_path, &rt.cfg) != 0) {
        HUB_LOGE("load config failed: %s", cfg_path);
        return 1;
    }

    HUB_LOGI("hub start, mqtt=%s:%d, video=%s", rt.cfg.mqtt_host, rt.cfg.mqtt_port, rt.cfg.video_dev);

    if (hub_device_hal_init(&rt.hal, &rt.cfg) != 0) {
        HUB_LOGE("device hal init failed");
        return 1;
    }

    if (hub_mem_pool_init(&rt.pool, (size_t)rt.cfg.pool_block_size, rt.cfg.pool_block_count) != 0) {
        HUB_LOGE("memory pool init failed");
        hub_device_hal_deinit(&rt.hal);
        return 1;
    }

    rt.rpc.hal = &rt.hal;
    if (hub_media_player_init(&rt.media, rt.cfg.media_default_file) != 0) {
        HUB_LOGE("media player init failed");
        hub_mem_pool_destroy(&rt.pool);
        hub_device_hal_deinit(&rt.hal);
        return 1;
    }
    rt.rpc.media = &rt.media;

    if (hub_video_pipeline_start(&rt.video, &rt.cfg, &rt.pool, NULL, NULL) != 0) {
        HUB_LOGW("video pipeline start failed, continue without camera");
    }

    if (hub_mqtt_service_start(&rt.mqtt, &rt.cfg, &rt.hal, on_mqtt_message, &rt) != 0) {
        HUB_LOGE("mqtt service start failed");
        hub_video_pipeline_stop(&rt.video);
        hub_media_player_deinit(&rt.media);
        hub_mem_pool_destroy(&rt.pool);
        hub_device_hal_deinit(&rt.hal);
        return 1;
    }

    while (!g_stop) {
        sleep(1);
    }

    HUB_LOGI("hub stopping...");
    hub_mqtt_service_stop(&rt.mqtt);
    hub_video_pipeline_stop(&rt.video);
    hub_media_player_deinit(&rt.media);
    hub_mem_pool_shutdown(&rt.pool);
    hub_mem_pool_destroy(&rt.pool);
    hub_device_hal_deinit(&rt.hal);
    HUB_LOGI("hub stopped");
    return 0;
}
