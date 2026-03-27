#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_service.h"
#include "edt_common.h"
#include "edt_time.h"
#include "protocol_decoder.h"

static volatile sig_atomic_t g_stop = 0;

static void edt_on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void edt_print_status(const edt_service_status_t *status)
{
    char ts[64];
    edt_format_local_time(status->timestamp_ns, ts, sizeof(ts));
    printf("[%s] sample_rate=%uHz total=%" PRIu64 " dropped=%" PRIu64 "\n",
           ts,
           status->sample_rate_hz,
           status->total_samples,
           status->dropped_samples);
    printf("  net: pkts=%" PRIu64 " bytes=%" PRIu64 " tcp=%" PRIu64 " udp=%" PRIu64 "\n",
           status->net_stats.packets_total,
           status->net_stats.bytes_total,
           status->net_stats.tcp_packets,
           status->net_stats.udp_packets);
    printf("  sys: cpu=%.2f%% mem=%" PRIu64 "/%" PRIu64 "KB rx=%" PRIu64 " tx=%" PRIu64 "\n",
           status->sys_stats.cpu_usage_percent,
           status->sys_stats.mem_used_kb,
           status->sys_stats.mem_total_kb,
           status->sys_stats.net_rx_bytes,
           status->sys_stats.net_tx_bytes);
    printf("  v4l2: %s driver=%s card=%s\n",
           status->v4l2_status.available ? "ready" : "unavailable",
           status->v4l2_status.driver,
           status->v4l2_status.card);
}

static void edt_print_events(const char *name, const edt_proto_event_t *events, size_t count, size_t limit)
{
    size_t i;
    if (count == 0U) {
        return;
    }
    if (count > limit) {
        count = limit;
    }
    printf("  %s events:\n", name);
    for (i = 0U; i < count; i++) {
        char ts[64];
        edt_format_local_time(events[i].timestamp_ns, ts, sizeof(ts));
        printf("    [%s] %s\n", ts, events[i].summary);
    }
}

int main(int argc, char **argv)
{
    edt_debug_service_t service;
    edt_service_config_t cfg;
    edt_service_status_t status;
    edt_logic_sample_t samples[8192];
    edt_proto_event_t uart_events[64];
    edt_proto_event_t i2c_events[64];
    edt_proto_event_t spi_events[64];
    edt_uart_decoder_config_t uart_cfg;
    edt_i2c_decoder_config_t i2c_cfg;
    edt_spi_decoder_config_t spi_cfg;
    int duration_sec = 10;
    int loop_count = 0;
    int max_loops;

    memset(&cfg, 0, sizeof(cfg));
    cfg.logic.sample_rate_hz = 5000000U;
    cfg.logic.max_sample_rate_hz = EDT_MAX_SAMPLE_RATE_HZ;
    cfg.logic.ring_capacity_samples = 1U << 17U;
    cfg.logic.simulate_signal = 1;
    cfg.sump.bind_ip = "0.0.0.0";
    cfg.sump.port = 9527U;
    cfg.sump.capture_samples = 8192U;
    cfg.net_capture.interface_name = "eth0";
    cfg.net_capture.promiscuous = 0;
    cfg.system_monitor.interval_ms = 500U;
    cfg.system_monitor.network_iface = "eth0";
    cfg.v4l2_device = "/dev/video0";
    cfg.plugin_dir = "./plugins";
    cfg.enable_sump_server = 1;
    cfg.enable_net_capture = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-sump") == 0) {
            cfg.enable_sump_server = 0;
        } else if (strcmp(argv[i], "--no-netcap") == 0) {
            cfg.enable_net_capture = 0;
        } else if (strcmp(argv[i], "--iface") == 0 && i + 1 < argc) {
            cfg.net_capture.interface_name = argv[++i];
            cfg.system_monitor.network_iface = cfg.net_capture.interface_name;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_sec = atoi(argv[++i]);
            if (duration_sec <= 0) {
                duration_sec = 10;
            }
        } else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            cfg.logic.sample_rate_hz = (uint32_t)strtoul(argv[++i], NULL, 10);
            if (cfg.logic.sample_rate_hz == 0U) {
                cfg.logic.sample_rate_hz = 1000000U;
            }
        }
    }

    if (edt_debug_service_init(&service, &cfg) != 0) {
        fprintf(stderr, "Failed to init debug service\n");
        return 1;
    }
    if (edt_debug_service_start(&service) != 0) {
        fprintf(stderr, "Failed to start debug service\n");
        edt_debug_service_destroy(&service);
        return 1;
    }

    uart_cfg.rx_channel = 0U;
    uart_cfg.baudrate = 115200U;
    uart_cfg.sample_rate_hz = edt_logic_analyzer_get_sample_rate(&service.logic);

    i2c_cfg.scl_channel = 1U;
    i2c_cfg.sda_channel = 2U;

    spi_cfg.clk_channel = 3U;
    spi_cfg.cs_channel = 6U;
    spi_cfg.mosi_channel = 4U;
    spi_cfg.miso_channel = 5U;
    spi_cfg.cpol = 0U;
    spi_cfg.cpha = 0U;

    (void)signal(SIGINT, edt_on_signal);
    (void)signal(SIGTERM, edt_on_signal);

    max_loops = duration_sec * 2;
    printf("Embedded Debug Toolkit started. Press Ctrl+C to stop.\n");
    printf("SUMP bridge: tcp://%s:%u\n", service.config.sump.bind_ip, service.config.sump.port);

    while (!g_stop && loop_count < max_loops) {
        size_t sample_count;
        size_t uart_count;
        size_t i2c_count;
        size_t spi_count;

        (void)edt_debug_service_get_status(&service, &status);
        edt_print_status(&status);

        sample_count = edt_logic_analyzer_capture_blocking(&service.logic, samples, EDT_ARRAY_SIZE(samples), 120U);
        uart_cfg.sample_rate_hz = edt_logic_analyzer_get_sample_rate(&service.logic);
        uart_count = edt_decode_uart(samples, sample_count, &uart_cfg, uart_events, EDT_ARRAY_SIZE(uart_events));
        i2c_count = edt_decode_i2c(samples, sample_count, &i2c_cfg, i2c_events, EDT_ARRAY_SIZE(i2c_events));
        spi_count = edt_decode_spi(samples, sample_count, &spi_cfg, spi_events, EDT_ARRAY_SIZE(spi_events));

        edt_print_events("UART", uart_events, uart_count, 4U);
        edt_print_events("I2C", i2c_events, i2c_count, 4U);
        edt_print_events("SPI", spi_events, spi_count, 4U);

        (void)edt_debug_service_poll_plugins(&service);
        (void)edt_sleep_ns(500000000ULL);
        loop_count++;
    }

    edt_debug_service_destroy(&service);
    return 0;
}
