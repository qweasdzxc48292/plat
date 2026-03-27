#ifndef EDT_SUMP_SERVER_H
#define EDT_SUMP_SERVER_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "logic_analyzer.h"

typedef struct {
    const char *bind_ip;
    uint16_t port;
    size_t capture_samples;
} edt_sump_server_config_t;

typedef struct {
    edt_sump_server_config_t config;
    edt_logic_analyzer_t *logic_analyzer;
    pthread_t listener_thread;
    atomic_int running;
    int listen_fd;
} edt_sump_server_t;

int edt_sump_server_init(edt_sump_server_t *server,
                         edt_logic_analyzer_t *logic_analyzer,
                         const edt_sump_server_config_t *config);
int edt_sump_server_start(edt_sump_server_t *server);
int edt_sump_server_stop(edt_sump_server_t *server);
void edt_sump_server_destroy(edt_sump_server_t *server);

#endif
