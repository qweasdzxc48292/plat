#include "sump_server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edt_common.h"

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define EDT_SUMP_CMD_RESET 0x00U
#define EDT_SUMP_CMD_ARM 0x01U
#define EDT_SUMP_CMD_ID 0x02U
#define EDT_SUMP_CMD_META 0x04U
#define EDT_SUMP_CMD_SET_DIVIDER 0x80U
#define EDT_SUMP_CMD_SET_READCOUNT 0x81U
#define EDT_SUMP_CMD_SET_TRIGGER_MASK 0xC0U
#define EDT_SUMP_CMD_SET_TRIGGER_VALUE 0xC1U
#define EDT_SUMP_CMD_SET_TRIGGER_MODE 0xC2U

typedef struct {
    edt_sump_server_t *server;
    int client_fd;
} edt_sump_client_ctx_t;

typedef struct {
    uint32_t divider;
    uint32_t read_count;
    uint8_t trigger_mask;
    uint8_t trigger_value;
    uint8_t trigger_mode;
} edt_sump_session_t;

#if defined(__linux__)
static int edt_send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0U;
    while (sent < len) {
        ssize_t ret = send(fd, buf + sent, len - sent, 0);
        if (ret <= 0) {
            return -errno;
        }
        sent += (size_t)ret;
    }
    return 0;
}

static int edt_recv_exact(int fd, uint8_t *buf, size_t len)
{
    size_t received = 0U;
    while (received < len) {
        ssize_t ret = recv(fd, buf + received, len - received, 0);
        if (ret <= 0) {
            return -errno;
        }
        received += (size_t)ret;
    }
    return 0;
}

static int edt_send_metadata(int fd, const edt_logic_analyzer_t *la)
{
    uint8_t block[64];
    size_t pos = 0U;
    const char *name = "Embedded Debug Toolkit";
    size_t name_len = strlen(name);

    block[pos++] = 0x01U;
    memcpy(block + pos, name, name_len + 1U);
    pos += name_len + 1U;

    block[pos++] = 0x21U;
    {
        uint32_t mem = (uint32_t)la->config.ring_capacity_samples;
        block[pos++] = (uint8_t)(mem & 0xFFU);
        block[pos++] = (uint8_t)((mem >> 8U) & 0xFFU);
        block[pos++] = (uint8_t)((mem >> 16U) & 0xFFU);
        block[pos++] = (uint8_t)((mem >> 24U) & 0xFFU);
    }

    block[pos++] = 0x23U;
    {
        uint32_t rate = la->config.max_sample_rate_hz;
        block[pos++] = (uint8_t)(rate & 0xFFU);
        block[pos++] = (uint8_t)((rate >> 8U) & 0xFFU);
        block[pos++] = (uint8_t)((rate >> 16U) & 0xFFU);
        block[pos++] = (uint8_t)((rate >> 24U) & 0xFFU);
    }

    block[pos++] = 0x40U;
    block[pos++] = 0x02U;

    block[pos++] = 0x00U;
    return edt_send_all(fd, block, pos);
}

static edt_trigger_mode_t edt_map_trigger_mode(uint8_t mode)
{
    switch (mode) {
    case 1U:
        return EDT_TRIGGER_EDGE_RISING;
    case 2U:
        return EDT_TRIGGER_EDGE_FALLING;
    case 3U:
        return EDT_TRIGGER_LEVEL_HIGH;
    case 4U:
        return EDT_TRIGGER_LEVEL_LOW;
    default:
        return EDT_TRIGGER_NONE;
    }
}

static void edt_sump_apply_session(edt_sump_server_t *server, const edt_sump_session_t *session)
{
    edt_trigger_config_t trigger;
    uint32_t sample_rate;
    uint8_t channel = 0U;
    edt_trigger_mode_t mode;

    if (session->divider == 0U) {
        sample_rate = server->logic_analyzer->config.max_sample_rate_hz;
    } else {
        sample_rate = server->logic_analyzer->config.max_sample_rate_hz / (session->divider + 1U);
    }
    if (sample_rate == 0U) {
        sample_rate = 1U;
    }

    (void)edt_logic_analyzer_set_sample_rate(server->logic_analyzer, sample_rate);

    if (session->trigger_mask != 0U) {
        while (channel < 8U) {
            if (((session->trigger_mask >> channel) & 0x1U) != 0U) {
                break;
            }
            channel++;
        }
    }
    if (channel >= 8U) {
        channel = 0U;
    }

    mode = edt_map_trigger_mode(session->trigger_mode);
    if (mode == EDT_TRIGGER_NONE && session->trigger_mask != 0U) {
        mode = (((session->trigger_value >> channel) & 0x1U) != 0U) ? EDT_TRIGGER_LEVEL_HIGH
                                                                     : EDT_TRIGGER_LEVEL_LOW;
    }
    trigger.mode = mode;
    trigger.channel = channel;
    (void)edt_logic_analyzer_set_trigger(server->logic_analyzer, &trigger);
    (void)edt_logic_analyzer_arm(server->logic_analyzer);
}

static int edt_sump_stream_capture(edt_sump_server_t *server, int client_fd, const edt_sump_session_t *session)
{
    size_t target = session->read_count ? (size_t)(session->read_count + 1U) : server->config.capture_samples;
    edt_logic_sample_t *samples;
    uint8_t *raw_bytes;
    size_t i;
    size_t got;
    int ret;

    if (target == 0U) {
        target = 4096U;
    }
    if (target > 1U << 20U) {
        target = 1U << 20U;
    }

    samples = (edt_logic_sample_t *)calloc(target, sizeof(*samples));
    raw_bytes = (uint8_t *)calloc(target, sizeof(uint8_t));
    if (!samples || !raw_bytes) {
        free(samples);
        free(raw_bytes);
        return -ENOMEM;
    }

    edt_sump_apply_session(server, session);
    got = edt_logic_analyzer_capture_blocking(server->logic_analyzer, samples, target, 1200U);
    for (i = 0U; i < got; i++) {
        raw_bytes[i] = samples[i].channels;
    }
    ret = edt_send_all(client_fd, raw_bytes, got);

    free(samples);
    free(raw_bytes);
    return ret;
}

static void *edt_sump_client_thread(void *arg)
{
    edt_sump_client_ctx_t *ctx = (edt_sump_client_ctx_t *)arg;
    edt_sump_session_t session;
    uint8_t cmd;
    int running = 1;

    memset(&session, 0, sizeof(session));
    session.read_count = (uint32_t)(ctx->server->config.capture_samples > 0U
                                        ? (ctx->server->config.capture_samples - 1U)
                                        : 4095U);

    while (running) {
        ssize_t r = recv(ctx->client_fd, &cmd, 1, 0);
        if (r <= 0) {
            break;
        }

        if (cmd == EDT_SUMP_CMD_RESET) {
            session.divider = 0U;
            session.read_count = (uint32_t)(ctx->server->config.capture_samples > 0U
                                                ? (ctx->server->config.capture_samples - 1U)
                                                : 4095U);
            session.trigger_mask = 0U;
            session.trigger_value = 0U;
            session.trigger_mode = 0U;
            continue;
        }
        if (cmd == EDT_SUMP_CMD_ID) {
            static const uint8_t id[] = {'1', 'A', 'L', 'S'};
            (void)edt_send_all(ctx->client_fd, id, sizeof(id));
            continue;
        }
        if (cmd == EDT_SUMP_CMD_META) {
            (void)edt_send_metadata(ctx->client_fd, ctx->server->logic_analyzer);
            continue;
        }
        if (cmd == EDT_SUMP_CMD_ARM) {
            if (edt_sump_stream_capture(ctx->server, ctx->client_fd, &session) != 0) {
                break;
            }
            continue;
        }
        if (cmd >= 0x80U) {
            uint8_t param_bytes[4];
            uint32_t param;
            if (edt_recv_exact(ctx->client_fd, param_bytes, sizeof(param_bytes)) != 0) {
                break;
            }
            param = ((uint32_t)param_bytes[0]) | ((uint32_t)param_bytes[1] << 8U) |
                    ((uint32_t)param_bytes[2] << 16U) | ((uint32_t)param_bytes[3] << 24U);
            switch (cmd) {
            case EDT_SUMP_CMD_SET_DIVIDER:
                session.divider = param;
                break;
            case EDT_SUMP_CMD_SET_READCOUNT:
                session.read_count = param;
                break;
            case EDT_SUMP_CMD_SET_TRIGGER_MASK:
                session.trigger_mask = (uint8_t)param;
                break;
            case EDT_SUMP_CMD_SET_TRIGGER_VALUE:
                session.trigger_value = (uint8_t)param;
                break;
            case EDT_SUMP_CMD_SET_TRIGGER_MODE:
                session.trigger_mode = (uint8_t)(param & 0xFFU);
                break;
            default:
                break;
            }
            continue;
        }
        running = 0;
    }

    (void)close(ctx->client_fd);
    free(ctx);
    return NULL;
}

static void *edt_sump_listener_thread(void *arg)
{
    edt_sump_server_t *server = (edt_sump_server_t *)arg;

    while (atomic_load_explicit(&server->running, memory_order_acquire) != 0) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (atomic_load_explicit(&server->running, memory_order_acquire) == 0) {
                break;
            }
            continue;
        }

        edt_sump_client_ctx_t *ctx = (edt_sump_client_ctx_t *)calloc(1U, sizeof(*ctx));
        pthread_t tid;
        if (!ctx) {
            (void)close(client_fd);
            continue;
        }
        ctx->server = server;
        ctx->client_fd = client_fd;
        if (pthread_create(&tid, NULL, edt_sump_client_thread, ctx) != 0) {
            free(ctx);
            (void)close(client_fd);
            continue;
        }
        (void)pthread_detach(tid);
    }

    return NULL;
}
#endif

int edt_sump_server_init(edt_sump_server_t *server,
                         edt_logic_analyzer_t *logic_analyzer,
                         const edt_sump_server_config_t *config)
{
    if (!server || !logic_analyzer) {
        return -EINVAL;
    }
    memset(server, 0, sizeof(*server));
    server->logic_analyzer = logic_analyzer;
    server->config.bind_ip = (config && config->bind_ip) ? config->bind_ip : "0.0.0.0";
    server->config.port = (config && config->port != 0U) ? config->port : 9527U;
    server->config.capture_samples = (config && config->capture_samples != 0U) ? config->capture_samples : 4096U;
    atomic_store(&server->running, 0);
    server->listen_fd = -1;
    return 0;
}

int edt_sump_server_start(edt_sump_server_t *server)
{
#if !defined(__linux__)
    (void)server;
    return -ENOSYS;
#else
    struct sockaddr_in addr;
    int enable = 1;

    if (!server) {
        return -EINVAL;
    }
    if (atomic_load(&server->running) != 0) {
        return 0;
    }

    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        return -errno;
    }
    (void)setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    if (inet_pton(AF_INET, server->config.bind_ip, &addr.sin_addr) <= 0) {
        (void)close(server->listen_fd);
        server->listen_fd = -1;
        return -EINVAL;
    }
    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        int err = errno;
        (void)close(server->listen_fd);
        server->listen_fd = -1;
        return -err;
    }
    if (listen(server->listen_fd, 8) != 0) {
        int err = errno;
        (void)close(server->listen_fd);
        server->listen_fd = -1;
        return -err;
    }

    atomic_store(&server->running, 1);
    if (pthread_create(&server->listener_thread, NULL, edt_sump_listener_thread, server) != 0) {
        int err = errno;
        atomic_store(&server->running, 0);
        (void)close(server->listen_fd);
        server->listen_fd = -1;
        return -err;
    }
    return 0;
#endif
}

int edt_sump_server_stop(edt_sump_server_t *server)
{
    if (!server) {
        return -EINVAL;
    }
    if (atomic_load(&server->running) == 0) {
        return 0;
    }
    atomic_store(&server->running, 0);
#if defined(__linux__)
    if (server->listen_fd >= 0) {
        (void)shutdown(server->listen_fd, SHUT_RDWR);
        (void)close(server->listen_fd);
        server->listen_fd = -1;
    }
    (void)pthread_join(server->listener_thread, NULL);
#endif
    return 0;
}

void edt_sump_server_destroy(edt_sump_server_t *server)
{
    if (!server) {
        return;
    }
    (void)edt_sump_server_stop(server);
}
