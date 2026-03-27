#include "tool_modules.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edt_time.h"

#if defined(__linux__)
#include <pthread.h>
#endif

static int edt_read_cpu(uint64_t *total, uint64_t *idle)
{
#if !defined(__linux__)
    (void)total;
    (void)idle;
    return -ENOSYS;
#else
    FILE *fp;
    char line[256];
    uint64_t user, nice, system, idle_v, iowait, irq, softirq, steal;

    fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -errno;
    }
    if (!fgets(line, sizeof(line), fp)) {
        (void)fclose(fp);
        return -EIO;
    }
    (void)fclose(fp);
    user = nice = system = idle_v = iowait = irq = softirq = steal = 0ULL;
    (void)sscanf(line,
                 "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                 &user,
                 &nice,
                 &system,
                 &idle_v,
                 &iowait,
                 &irq,
                 &softirq,
                 &steal);
    *idle = idle_v + iowait;
    *total = user + nice + system + idle_v + iowait + irq + softirq + steal;
    return 0;
#endif
}

static int edt_read_mem(uint64_t *mem_total, uint64_t *mem_free)
{
#if !defined(__linux__)
    (void)mem_total;
    (void)mem_free;
    return -ENOSYS;
#else
    FILE *fp;
    char key[64];
    uint64_t value;
    char unit[32];
    uint64_t total = 0ULL;
    uint64_t available = 0ULL;

    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -errno;
    }
    while (fscanf(fp, "%63s %llu %31s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) {
            total = value;
        } else if (strcmp(key, "MemAvailable:") == 0) {
            available = value;
        }
    }
    (void)fclose(fp);

    *mem_total = total;
    *mem_free = available;
    return 0;
#endif
}

static int edt_read_net(const char *iface, uint64_t *rx_bytes, uint64_t *tx_bytes)
{
#if !defined(__linux__)
    (void)iface;
    (void)rx_bytes;
    (void)tx_bytes;
    return -ENOSYS;
#else
    FILE *fp;
    char line[512];
    size_t iface_len;

    if (!iface) {
        return -EINVAL;
    }

    iface_len = strlen(iface);
    fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return -errno;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *iface_name = line;
        char *colon = strchr(line, ':');
        if (!colon) {
            continue;
        }
        *colon = '\0';
        while (*iface_name == ' ') {
            iface_name++;
        }
        if (strncmp(iface_name, iface, iface_len) == 0 && iface_name[iface_len] == '\0') {
            uint64_t rx = 0ULL;
            uint64_t tx = 0ULL;
            char *stats = colon + 1;
            if (sscanf(stats,
                       "%llu %*u %*u %*u %*u %*u %*u %*u %llu",
                       &rx,
                       &tx) == 2) {
                *rx_bytes = rx;
                *tx_bytes = tx;
                (void)fclose(fp);
                return 0;
            }
        }
    }
    (void)fclose(fp);
    return -ENOENT;
#endif
}

static int edt_system_collect_once(edt_system_monitor_t *monitor, edt_system_snapshot_t *snapshot)
{
    uint64_t total;
    uint64_t idle;
    uint64_t mem_total;
    uint64_t mem_free;
    uint64_t rx = 0ULL;
    uint64_t tx = 0ULL;
    int ret_cpu;
    int ret_mem;

    ret_cpu = edt_read_cpu(&total, &idle);
    ret_mem = edt_read_mem(&mem_total, &mem_free);
    (void)edt_read_net(monitor->config.network_iface ? monitor->config.network_iface : "eth0", &rx, &tx);
    snapshot->timestamp_ns = edt_now_ns();

    if (ret_cpu == 0 && monitor->prev_cpu_total != 0ULL && total > monitor->prev_cpu_total) {
        const uint64_t delta_total = total - monitor->prev_cpu_total;
        const uint64_t delta_idle = idle - monitor->prev_cpu_idle;
        if (delta_total > 0ULL) {
            snapshot->cpu_usage_percent = (double)(delta_total - delta_idle) * 100.0 / (double)delta_total;
        }
    }
    monitor->prev_cpu_total = total;
    monitor->prev_cpu_idle = idle;

    if (ret_mem == 0) {
        snapshot->mem_total_kb = mem_total;
        snapshot->mem_free_kb = mem_free;
        snapshot->mem_used_kb = (mem_total >= mem_free) ? (mem_total - mem_free) : 0ULL;
    }
    snapshot->net_rx_bytes = rx;
    snapshot->net_tx_bytes = tx;
    return 0;
}

static void *edt_system_monitor_thread(void *arg)
{
    edt_system_monitor_t *monitor = (edt_system_monitor_t *)arg;
    uint32_t interval_ms = monitor->config.interval_ms;
    if (interval_ms == 0U) {
        interval_ms = 1000U;
    }

    while (atomic_load_explicit(&monitor->running, memory_order_acquire) != 0) {
        edt_system_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        (void)edt_system_collect_once(monitor, &snap);

        pthread_mutex_lock(&monitor->lock);
        monitor->latest = snap;
        pthread_mutex_unlock(&monitor->lock);

        (void)edt_sleep_ns((uint64_t)interval_ms * 1000000ULL);
    }
    return NULL;
}

int edt_system_monitor_init(edt_system_monitor_t *monitor, const edt_system_monitor_config_t *config)
{
    if (!monitor) {
        return -EINVAL;
    }
    memset(monitor, 0, sizeof(*monitor));
    if (config) {
        monitor->config = *config;
    } else {
        monitor->config.interval_ms = 1000U;
        monitor->config.network_iface = "eth0";
    }
    if (!monitor->config.network_iface) {
        monitor->config.network_iface = "eth0";
    }
    if (monitor->config.interval_ms == 0U) {
        monitor->config.interval_ms = 1000U;
    }
    if (pthread_mutex_init(&monitor->lock, NULL) != 0) {
        return -errno;
    }
    atomic_store(&monitor->running, 0);
    return 0;
}

int edt_system_monitor_start(edt_system_monitor_t *monitor)
{
    if (!monitor) {
        return -EINVAL;
    }
    if (atomic_load(&monitor->running) != 0) {
        return 0;
    }
    atomic_store(&monitor->running, 1);
    if (pthread_create(&monitor->thread, NULL, edt_system_monitor_thread, monitor) != 0) {
        int err = errno;
        atomic_store(&monitor->running, 0);
        return -err;
    }
    return 0;
}

int edt_system_monitor_stop(edt_system_monitor_t *monitor)
{
    if (!monitor) {
        return -EINVAL;
    }
    if (atomic_load(&monitor->running) == 0) {
        return 0;
    }
    atomic_store(&monitor->running, 0);
    (void)pthread_join(monitor->thread, NULL);
    return 0;
}

void edt_system_monitor_destroy(edt_system_monitor_t *monitor)
{
    if (!monitor) {
        return;
    }
    (void)edt_system_monitor_stop(monitor);
    (void)pthread_mutex_destroy(&monitor->lock);
}

int edt_system_monitor_get_latest(edt_system_monitor_t *monitor, edt_system_snapshot_t *out_snapshot)
{
    if (!monitor || !out_snapshot) {
        return -EINVAL;
    }
    pthread_mutex_lock(&monitor->lock);
    *out_snapshot = monitor->latest;
    pthread_mutex_unlock(&monitor->lock);
    return 0;
}
