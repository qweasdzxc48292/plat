#ifndef EDT_LOGIC_ANALYZER_H
#define EDT_LOGIC_ANALYZER_H

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "ring_buffer.h"

typedef enum {
    EDT_TRIGGER_NONE = 0,
    EDT_TRIGGER_EDGE_RISING,
    EDT_TRIGGER_EDGE_FALLING,
    EDT_TRIGGER_LEVEL_HIGH,
    EDT_TRIGGER_LEVEL_LOW
} edt_trigger_mode_t;

typedef struct {
    edt_trigger_mode_t mode;
    uint8_t channel;
} edt_trigger_config_t;

typedef struct {
    uint64_t timestamp_ns;
    uint8_t channels;
} edt_logic_sample_t;

typedef uint8_t (*edt_logic_reader_fn)(void *user_ctx);

typedef struct {
    uint32_t sample_rate_hz;
    uint32_t max_sample_rate_hz;
    size_t ring_capacity_samples;
    int simulate_signal;
} edt_logic_config_t;

typedef struct {
    edt_logic_config_t config;
    edt_ring_buffer_t ring;
    edt_trigger_config_t trigger;
    edt_logic_reader_fn reader;
    void *reader_ctx;

    pthread_t sample_thread;
    atomic_int running;
    atomic_int armed;
    atomic_uint_fast64_t total_samples;
    atomic_uint_fast64_t dropped_samples;

    uint8_t last_raw_value;
    uint64_t period_ns;
    uint64_t synthetic_counter;
} edt_logic_analyzer_t;

int edt_logic_analyzer_init(edt_logic_analyzer_t *la, const edt_logic_config_t *config);
void edt_logic_analyzer_destroy(edt_logic_analyzer_t *la);

int edt_logic_analyzer_start(edt_logic_analyzer_t *la);
int edt_logic_analyzer_stop(edt_logic_analyzer_t *la);

int edt_logic_analyzer_set_sample_rate(edt_logic_analyzer_t *la, uint32_t sample_rate_hz);
uint32_t edt_logic_analyzer_get_sample_rate(const edt_logic_analyzer_t *la);

int edt_logic_analyzer_set_trigger(edt_logic_analyzer_t *la, const edt_trigger_config_t *trigger);
int edt_logic_analyzer_arm(edt_logic_analyzer_t *la);

size_t edt_logic_analyzer_read_samples(edt_logic_analyzer_t *la,
                                       edt_logic_sample_t *out_samples,
                                       size_t max_samples);

size_t edt_logic_analyzer_capture_blocking(edt_logic_analyzer_t *la,
                                           edt_logic_sample_t *out_samples,
                                           size_t max_samples,
                                           uint32_t timeout_ms);

void edt_logic_analyzer_set_reader(edt_logic_analyzer_t *la, edt_logic_reader_fn reader, void *reader_ctx);

uint64_t edt_logic_analyzer_total_samples(const edt_logic_analyzer_t *la);
uint64_t edt_logic_analyzer_dropped_samples(const edt_logic_analyzer_t *la);

#endif
