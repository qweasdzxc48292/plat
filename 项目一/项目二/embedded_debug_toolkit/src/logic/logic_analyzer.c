#include "logic_analyzer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "edt_common.h"
#include "edt_time.h"

static uint8_t edt_synthetic_wave(uint64_t tick, uint32_t sample_rate_hz)
{
    uint8_t value = 0U;
    uint64_t half_period;

    if (sample_rate_hz == 0U) {
        return 0U;
    }

    half_period = (uint64_t)sample_rate_hz / 2000ULL;
    if (half_period == 0U) {
        half_period = 1U;
    }
    if (((tick / half_period) & 0x1ULL) != 0ULL) {
        value |= (1U << 0);
    }

    half_period = (uint64_t)sample_rate_hz / 10000ULL;
    if (half_period == 0U) {
        half_period = 1U;
    }
    if (((tick / half_period) & 0x1ULL) != 0ULL) {
        value |= (1U << 1);
    }

    half_period = (uint64_t)sample_rate_hz / 20000ULL;
    if (half_period == 0U) {
        half_period = 1U;
    }
    if (((tick / half_period) & 0x1ULL) != 0ULL) {
        value |= (1U << 2);
    }

    half_period = (uint64_t)sample_rate_hz / 500000ULL;
    if (half_period == 0U) {
        half_period = 1U;
    }
    if (((tick / half_period) & 0x1ULL) != 0ULL) {
        value |= (1U << 3);
    }

    if (((tick / 16ULL) & 0x1ULL) != 0ULL) {
        value |= (1U << 4);
    }
    if (((tick / 32ULL) & 0x1ULL) != 0ULL) {
        value |= (1U << 5);
    }
    if (((tick / 128ULL) & 0x1ULL) == 0ULL) {
        value |= (1U << 6);
    }
    if (((tick / 1024ULL) & 0x1ULL) != 0ULL) {
        value |= (1U << 7);
    }

    return value;
}

static int edt_trigger_matched(const edt_trigger_config_t *trigger, uint8_t previous, uint8_t current)
{
    const uint8_t bit_mask = (uint8_t)(1U << trigger->channel);
    const uint8_t prev_bit = (previous & bit_mask) ? 1U : 0U;
    const uint8_t curr_bit = (current & bit_mask) ? 1U : 0U;

    switch (trigger->mode) {
    case EDT_TRIGGER_NONE:
        return 1;
    case EDT_TRIGGER_EDGE_RISING:
        return prev_bit == 0U && curr_bit == 1U;
    case EDT_TRIGGER_EDGE_FALLING:
        return prev_bit == 1U && curr_bit == 0U;
    case EDT_TRIGGER_LEVEL_HIGH:
        return curr_bit == 1U;
    case EDT_TRIGGER_LEVEL_LOW:
        return curr_bit == 0U;
    default:
        return 0;
    }
}

static void *edt_logic_sample_thread(void *arg)
{
    edt_logic_analyzer_t *la = (edt_logic_analyzer_t *)arg;
    edt_logic_sample_t batch[256];
    size_t batch_target;
    uint64_t batch_period_ns;

    while (atomic_load_explicit(&la->running, memory_order_acquire) != 0) {
        size_t batch_count = 0U;
        uint64_t batch_start_ns = edt_now_ns();
        batch_target = (la->config.sample_rate_hz >= 1000000U) ? EDT_ARRAY_SIZE(batch) : 32U;
        batch_period_ns = la->period_ns * batch_target;

        while (batch_count < batch_target) {
            edt_logic_sample_t sample;
            int armed;
            uint8_t current_raw;

            if (la->reader) {
                current_raw = la->reader(la->reader_ctx);
            } else if (la->config.simulate_signal) {
                current_raw = edt_synthetic_wave(la->synthetic_counter, la->config.sample_rate_hz);
            } else {
                current_raw = 0U;
            }
            la->synthetic_counter++;
            sample.timestamp_ns = batch_start_ns + (uint64_t)batch_count * la->period_ns;
            sample.channels = current_raw;

            armed = atomic_load_explicit(&la->armed, memory_order_acquire);
            if (armed != 0) {
                if (edt_trigger_matched(&la->trigger, la->last_raw_value, current_raw)) {
                    atomic_store_explicit(&la->armed, 0, memory_order_release);
                    armed = 0;
                }
            }

            if (armed == 0) {
                batch[batch_count++] = sample;
            }

            la->last_raw_value = current_raw;
            atomic_fetch_add_explicit(&la->total_samples, 1ULL, memory_order_relaxed);
        }

        if (batch_count > 0U) {
            const size_t bytes_to_write = batch_count * sizeof(edt_logic_sample_t);
            const size_t bytes_written =
                edt_ring_buffer_write(&la->ring, (const uint8_t *)batch, bytes_to_write);
            if (bytes_written < bytes_to_write) {
                const uint64_t dropped = (bytes_to_write - bytes_written) / sizeof(edt_logic_sample_t);
                atomic_fetch_add_explicit(&la->dropped_samples, dropped, memory_order_relaxed);
            }
        }

        if (batch_period_ns >= 1000000ULL) {
            (void)edt_sleep_ns(batch_period_ns);
        } else if (la->config.sample_rate_hz <= 100000U) {
            (void)edt_sleep_ns(1000000ULL);
        }
    }
    return NULL;
}

int edt_logic_analyzer_init(edt_logic_analyzer_t *la, const edt_logic_config_t *config)
{
    edt_logic_config_t defaults;
    size_t ring_bytes;

    if (!la) {
        return -EINVAL;
    }
    memset(la, 0, sizeof(*la));

    defaults.sample_rate_hz = 1000000U;
    defaults.max_sample_rate_hz = EDT_MAX_SAMPLE_RATE_HZ;
    defaults.ring_capacity_samples = 1U << 16U;
    defaults.simulate_signal = 1;

    if (config) {
        la->config = *config;
    } else {
        la->config = defaults;
    }

    if (la->config.max_sample_rate_hz == 0U || la->config.max_sample_rate_hz > EDT_MAX_SAMPLE_RATE_HZ) {
        la->config.max_sample_rate_hz = EDT_MAX_SAMPLE_RATE_HZ;
    }
    if (la->config.sample_rate_hz == 0U) {
        la->config.sample_rate_hz = defaults.sample_rate_hz;
    }
    if (la->config.sample_rate_hz > la->config.max_sample_rate_hz) {
        la->config.sample_rate_hz = la->config.max_sample_rate_hz;
    }
    if (la->config.ring_capacity_samples == 0U) {
        la->config.ring_capacity_samples = defaults.ring_capacity_samples;
    }

    la->period_ns = 1000000000ULL / la->config.sample_rate_hz;
    if (la->period_ns == 0U) {
        la->period_ns = 1U;
    }
    la->trigger.mode = EDT_TRIGGER_NONE;
    la->trigger.channel = 0U;
    ring_bytes = la->config.ring_capacity_samples * sizeof(edt_logic_sample_t);
    if (edt_ring_buffer_init(&la->ring, ring_bytes) != 0) {
        return -ENOMEM;
    }
    atomic_store(&la->running, 0);
    atomic_store(&la->armed, 0);
    atomic_store(&la->total_samples, 0ULL);
    atomic_store(&la->dropped_samples, 0ULL);
    return 0;
}

void edt_logic_analyzer_destroy(edt_logic_analyzer_t *la)
{
    if (!la) {
        return;
    }
    (void)edt_logic_analyzer_stop(la);
    edt_ring_buffer_destroy(&la->ring);
}

int edt_logic_analyzer_start(edt_logic_analyzer_t *la)
{
    if (!la) {
        return -EINVAL;
    }
    if (atomic_load(&la->running) != 0) {
        return 0;
    }

    if (la->trigger.mode != EDT_TRIGGER_NONE) {
        atomic_store(&la->armed, 1);
    } else {
        atomic_store(&la->armed, 0);
    }

    atomic_store(&la->running, 1);
    if (pthread_create(&la->sample_thread, NULL, edt_logic_sample_thread, la) != 0) {
        atomic_store(&la->running, 0);
        return -errno;
    }
    return 0;
}

int edt_logic_analyzer_stop(edt_logic_analyzer_t *la)
{
    if (!la) {
        return -EINVAL;
    }
    if (atomic_load(&la->running) == 0) {
        return 0;
    }
    atomic_store(&la->running, 0);
    (void)pthread_join(la->sample_thread, NULL);
    return 0;
}

int edt_logic_analyzer_set_sample_rate(edt_logic_analyzer_t *la, uint32_t sample_rate_hz)
{
    if (!la || sample_rate_hz == 0U) {
        return -EINVAL;
    }
    if (sample_rate_hz > la->config.max_sample_rate_hz) {
        sample_rate_hz = la->config.max_sample_rate_hz;
    }
    la->config.sample_rate_hz = sample_rate_hz;
    la->period_ns = 1000000000ULL / sample_rate_hz;
    if (la->period_ns == 0U) {
        la->period_ns = 1U;
    }
    return 0;
}

uint32_t edt_logic_analyzer_get_sample_rate(const edt_logic_analyzer_t *la)
{
    if (!la) {
        return 0U;
    }
    return la->config.sample_rate_hz;
}

int edt_logic_analyzer_set_trigger(edt_logic_analyzer_t *la, const edt_trigger_config_t *trigger)
{
    if (!la || !trigger) {
        return -EINVAL;
    }
    if (trigger->channel >= EDT_MAX_CHANNELS) {
        return -EINVAL;
    }
    la->trigger = *trigger;
    if (trigger->mode == EDT_TRIGGER_NONE) {
        atomic_store(&la->armed, 0);
    } else {
        atomic_store(&la->armed, 1);
    }
    return 0;
}

int edt_logic_analyzer_arm(edt_logic_analyzer_t *la)
{
    if (!la) {
        return -EINVAL;
    }
    if (la->trigger.mode == EDT_TRIGGER_NONE) {
        return 0;
    }
    atomic_store(&la->armed, 1);
    return 0;
}

size_t edt_logic_analyzer_read_samples(edt_logic_analyzer_t *la,
                                       edt_logic_sample_t *out_samples,
                                       size_t max_samples)
{
    size_t bytes_read;
    size_t samples_read;

    if (!la || !out_samples || max_samples == 0U) {
        return 0U;
    }
    bytes_read = edt_ring_buffer_read(&la->ring,
                                      (uint8_t *)out_samples,
                                      max_samples * sizeof(edt_logic_sample_t));
    samples_read = bytes_read / sizeof(edt_logic_sample_t);
    return samples_read;
}

size_t edt_logic_analyzer_capture_blocking(edt_logic_analyzer_t *la,
                                           edt_logic_sample_t *out_samples,
                                           size_t max_samples,
                                           uint32_t timeout_ms)
{
    const uint64_t start_ns = edt_now_ns();
    const uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    size_t total = 0U;

    if (!la || !out_samples || max_samples == 0U) {
        return 0U;
    }

    while (total < max_samples) {
        size_t n = edt_logic_analyzer_read_samples(la, out_samples + total, max_samples - total);
        total += n;
        if (total >= max_samples) {
            break;
        }
        if (timeout_ms > 0U && (edt_now_ns() - start_ns) >= timeout_ns) {
            break;
        }
        (void)edt_sleep_ns(1000000ULL);
    }
    return total;
}

void edt_logic_analyzer_set_reader(edt_logic_analyzer_t *la, edt_logic_reader_fn reader, void *reader_ctx)
{
    if (!la) {
        return;
    }
    la->reader = reader;
    la->reader_ctx = reader_ctx;
}

uint64_t edt_logic_analyzer_total_samples(const edt_logic_analyzer_t *la)
{
    if (!la) {
        return 0ULL;
    }
    return atomic_load(&la->total_samples);
}

uint64_t edt_logic_analyzer_dropped_samples(const edt_logic_analyzer_t *la)
{
    if (!la) {
        return 0ULL;
    }
    return atomic_load(&la->dropped_samples);
}
