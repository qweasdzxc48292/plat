#include "debug_service.h"

#include <errno.h>
#include <string.h>

#include "edt_common.h"
#include "edt_time.h"

static void edt_apply_default_config(edt_service_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->logic.sample_rate_hz = 1000000U;
    cfg->logic.max_sample_rate_hz = EDT_MAX_SAMPLE_RATE_HZ;
    cfg->logic.ring_capacity_samples = 1U << 16U;
    cfg->logic.simulate_signal = 1;

    cfg->sump.bind_ip = "0.0.0.0";
    cfg->sump.port = 9527U;
    cfg->sump.capture_samples = 8192U;

    cfg->net_capture.interface_name = "eth0";
    cfg->net_capture.promiscuous = 0;

    cfg->system_monitor.interval_ms = 1000U;
    cfg->system_monitor.network_iface = "eth0";

    cfg->v4l2_device = "/dev/video0";
    cfg->plugin_dir = "./plugins";
    cfg->enable_sump_server = 1;
    cfg->enable_net_capture = 1;
}

int edt_debug_service_init(edt_debug_service_t *service, const edt_service_config_t *config)
{
    int ret;
    edt_service_config_t merged;

    if (!service) {
        return -EINVAL;
    }
    memset(service, 0, sizeof(*service));
    edt_apply_default_config(&merged);
    if (config) {
        merged = *config;
        if (merged.logic.max_sample_rate_hz == 0U) {
            merged.logic.max_sample_rate_hz = EDT_MAX_SAMPLE_RATE_HZ;
        }
        if (merged.logic.sample_rate_hz == 0U) {
            merged.logic.sample_rate_hz = 1000000U;
        }
        if (merged.logic.ring_capacity_samples == 0U) {
            merged.logic.ring_capacity_samples = 1U << 16U;
        }
        if (!merged.sump.bind_ip) {
            merged.sump.bind_ip = "0.0.0.0";
        }
        if (merged.sump.port == 0U) {
            merged.sump.port = 9527U;
        }
        if (merged.sump.capture_samples == 0U) {
            merged.sump.capture_samples = 8192U;
        }
        if (!merged.net_capture.interface_name) {
            merged.net_capture.interface_name = "eth0";
        }
        if (merged.system_monitor.interval_ms == 0U) {
            merged.system_monitor.interval_ms = 1000U;
        }
        if (!merged.system_monitor.network_iface) {
            merged.system_monitor.network_iface = merged.net_capture.interface_name;
        }
        if (!merged.v4l2_device) {
            merged.v4l2_device = "/dev/video0";
        }
        if (!merged.plugin_dir) {
            merged.plugin_dir = "./plugins";
        }
    }
    service->config = merged;

    ret = edt_logic_analyzer_init(&service->logic, &service->config.logic);
    if (ret != 0) {
        return ret;
    }

    ret = edt_sump_server_init(&service->sump_server, &service->logic, &service->config.sump);
    if (ret != 0) {
        edt_logic_analyzer_destroy(&service->logic);
        return ret;
    }

    ret = edt_net_capture_init(&service->net_capture, &service->config.net_capture);
    if (ret != 0) {
        edt_logic_analyzer_destroy(&service->logic);
        return ret;
    }

    ret = edt_system_monitor_init(&service->system_monitor, &service->config.system_monitor);
    if (ret != 0) {
        edt_net_capture_destroy(&service->net_capture);
        edt_logic_analyzer_destroy(&service->logic);
        return ret;
    }

    (void)edt_v4l2_probe(service->config.v4l2_device, &service->v4l2_status);

    ret = edt_plugin_manager_init(&service->plugin_manager, 16U, service);
    if (ret != 0) {
        edt_system_monitor_destroy(&service->system_monitor);
        edt_net_capture_destroy(&service->net_capture);
        edt_logic_analyzer_destroy(&service->logic);
        return ret;
    }
    (void)edt_plugin_manager_load_dir(&service->plugin_manager, service->config.plugin_dir);
    service->started = 0;
    return 0;
}

int edt_debug_service_start(edt_debug_service_t *service)
{
    int ret;

    if (!service) {
        return -EINVAL;
    }
    if (service->started) {
        return 0;
    }

    ret = edt_logic_analyzer_start(&service->logic);
    if (ret != 0) {
        return ret;
    }

    ret = edt_system_monitor_start(&service->system_monitor);
    if (ret != 0) {
        (void)edt_logic_analyzer_stop(&service->logic);
        return ret;
    }

    if (service->config.enable_net_capture) {
        if (edt_net_capture_start(&service->net_capture) != 0) {
            service->config.enable_net_capture = 0;
        }
    }

    if (service->config.enable_sump_server) {
        if (edt_sump_server_start(&service->sump_server) != 0) {
            service->config.enable_sump_server = 0;
        }
    }

    service->started = 1;
    return 0;
}

int edt_debug_service_stop(edt_debug_service_t *service)
{
    if (!service) {
        return -EINVAL;
    }
    if (!service->started) {
        return 0;
    }

    (void)edt_sump_server_stop(&service->sump_server);
    (void)edt_net_capture_stop(&service->net_capture);
    (void)edt_system_monitor_stop(&service->system_monitor);
    (void)edt_logic_analyzer_stop(&service->logic);
    service->started = 0;
    return 0;
}

void edt_debug_service_destroy(edt_debug_service_t *service)
{
    if (!service) {
        return;
    }
    (void)edt_debug_service_stop(service);
    edt_plugin_manager_destroy(&service->plugin_manager);
    edt_system_monitor_destroy(&service->system_monitor);
    edt_net_capture_destroy(&service->net_capture);
    edt_sump_server_destroy(&service->sump_server);
    edt_logic_analyzer_destroy(&service->logic);
}

int edt_debug_service_get_status(edt_debug_service_t *service, edt_service_status_t *status)
{
    if (!service || !status) {
        return -EINVAL;
    }

    memset(status, 0, sizeof(*status));
    status->timestamp_ns = edt_now_ns();
    status->sample_rate_hz = edt_logic_analyzer_get_sample_rate(&service->logic);
    status->total_samples = edt_logic_analyzer_total_samples(&service->logic);
    status->dropped_samples = edt_logic_analyzer_dropped_samples(&service->logic);
    status->v4l2_status = service->v4l2_status;
    (void)edt_net_capture_get_stats(&service->net_capture, &status->net_stats);
    (void)edt_system_monitor_get_latest(&service->system_monitor, &status->sys_stats);
    return 0;
}

int edt_debug_service_poll_plugins(edt_debug_service_t *service)
{
    if (!service) {
        return -EINVAL;
    }
    return edt_plugin_manager_tick(&service->plugin_manager);
}
