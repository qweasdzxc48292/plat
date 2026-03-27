#ifndef EDT_DEBUG_SERVICE_H
#define EDT_DEBUG_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "logic_analyzer.h"
#include "plugin_manager.h"
#include "sump_server.h"
#include "tool_modules.h"

typedef struct {
    edt_logic_config_t logic;
    edt_sump_server_config_t sump;
    edt_net_capture_config_t net_capture;
    edt_system_monitor_config_t system_monitor;
    const char *v4l2_device;
    const char *plugin_dir;
    int enable_sump_server;
    int enable_net_capture;
} edt_service_config_t;

typedef struct {
    uint64_t timestamp_ns;
    uint32_t sample_rate_hz;
    uint64_t total_samples;
    uint64_t dropped_samples;
    edt_net_capture_stats_t net_stats;
    edt_system_snapshot_t sys_stats;
    edt_v4l2_probe_result_t v4l2_status;
} edt_service_status_t;

typedef struct {
    edt_service_config_t config;
    edt_logic_analyzer_t logic;
    edt_sump_server_t sump_server;
    edt_net_capture_t net_capture;
    edt_system_monitor_t system_monitor;
    edt_v4l2_probe_result_t v4l2_status;
    edt_plugin_manager_t plugin_manager;
    int started;
} edt_debug_service_t;

int edt_debug_service_init(edt_debug_service_t *service, const edt_service_config_t *config);
int edt_debug_service_start(edt_debug_service_t *service);
int edt_debug_service_stop(edt_debug_service_t *service);
void edt_debug_service_destroy(edt_debug_service_t *service);

int edt_debug_service_get_status(edt_debug_service_t *service, edt_service_status_t *status);
int edt_debug_service_poll_plugins(edt_debug_service_t *service);

#endif
