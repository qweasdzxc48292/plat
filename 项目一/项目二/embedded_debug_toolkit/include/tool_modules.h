#ifndef EDT_TOOL_MODULES_H
#define EDT_TOOL_MODULES_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int available;
    char device[64];
    char driver[64];
    char card[64];
    uint32_t capabilities;
    uint64_t timestamp_ns;
} edt_v4l2_probe_result_t;

int edt_v4l2_probe(const char *device, edt_v4l2_probe_result_t *result);
int edt_v4l2_dump_formats(const char *device, char *buffer, size_t buffer_size);

typedef struct {
    const char *interface_name;
    int promiscuous;
} edt_net_capture_config_t;

typedef struct {
    uint64_t timestamp_ns;
    uint64_t packets_total;
    uint64_t bytes_total;
    uint64_t ipv4_packets;
    uint64_t arp_packets;
    uint64_t tcp_packets;
    uint64_t udp_packets;
    uint64_t icmp_packets;
    uint64_t other_packets;
} edt_net_capture_stats_t;

typedef struct {
    edt_net_capture_config_t config;
    pthread_t thread;
    atomic_int running;
    pthread_mutex_t lock;
    edt_net_capture_stats_t stats;
    int socket_fd;
} edt_net_capture_t;

int edt_net_capture_init(edt_net_capture_t *capture, const edt_net_capture_config_t *config);
int edt_net_capture_start(edt_net_capture_t *capture);
int edt_net_capture_stop(edt_net_capture_t *capture);
void edt_net_capture_destroy(edt_net_capture_t *capture);
int edt_net_capture_get_stats(edt_net_capture_t *capture, edt_net_capture_stats_t *out_stats);

typedef struct {
    uint32_t interval_ms;
    const char *network_iface;
} edt_system_monitor_config_t;

typedef struct {
    uint64_t timestamp_ns;
    double cpu_usage_percent;
    uint64_t mem_total_kb;
    uint64_t mem_used_kb;
    uint64_t mem_free_kb;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
} edt_system_snapshot_t;

typedef struct {
    edt_system_monitor_config_t config;
    pthread_t thread;
    atomic_int running;
    pthread_mutex_t lock;
    edt_system_snapshot_t latest;
    uint64_t prev_cpu_total;
    uint64_t prev_cpu_idle;
} edt_system_monitor_t;

int edt_system_monitor_init(edt_system_monitor_t *monitor, const edt_system_monitor_config_t *config);
int edt_system_monitor_start(edt_system_monitor_t *monitor);
int edt_system_monitor_stop(edt_system_monitor_t *monitor);
void edt_system_monitor_destroy(edt_system_monitor_t *monitor);
int edt_system_monitor_get_latest(edt_system_monitor_t *monitor, edt_system_snapshot_t *out_snapshot);

#endif
