#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "protocol_decoder.h"

int test_ring_buffer(void);

static size_t build_uart_samples(edt_logic_sample_t *samples,
                                 size_t max_samples,
                                 uint8_t data,
                                 uint32_t sample_rate,
                                 uint32_t baudrate)
{
    size_t pos = 0U;
    size_t spb = sample_rate / baudrate;
    size_t i;
    uint64_t ts = 0ULL;

    if (spb == 0U) {
        return 0U;
    }

    for (i = 0U; i < spb * 2U && pos < max_samples; i++) {
        samples[pos].timestamp_ns = ts;
        samples[pos].channels = 0x01U;
        pos++;
        ts += 1000000000ULL / sample_rate;
    }

    for (i = 0U; i < spb && pos < max_samples; i++) {
        samples[pos].timestamp_ns = ts;
        samples[pos].channels = 0x00U;
        pos++;
        ts += 1000000000ULL / sample_rate;
    }

    for (size_t bit = 0U; bit < 8U; bit++) {
        uint8_t b = (uint8_t)((data >> bit) & 0x1U);
        for (i = 0U; i < spb && pos < max_samples; i++) {
            samples[pos].timestamp_ns = ts;
            samples[pos].channels = b ? 0x01U : 0x00U;
            pos++;
            ts += 1000000000ULL / sample_rate;
        }
    }

    for (i = 0U; i < spb && pos < max_samples; i++) {
        samples[pos].timestamp_ns = ts;
        samples[pos].channels = 0x01U;
        pos++;
        ts += 1000000000ULL / sample_rate;
    }
    return pos;
}

static void test_uart_decoder(void)
{
    edt_logic_sample_t samples[4096];
    edt_proto_event_t events[16];
    edt_uart_decoder_config_t cfg;
    size_t sample_count;
    size_t event_count;

    memset(samples, 0, sizeof(samples));
    memset(events, 0, sizeof(events));

    cfg.rx_channel = 0U;
    cfg.baudrate = 9600U;
    cfg.sample_rate_hz = 1000000U;

    sample_count = build_uart_samples(samples, 4096U, 0xA5U, cfg.sample_rate_hz, cfg.baudrate);
    event_count = edt_decode_uart(samples, sample_count, &cfg, events, 16U);
    assert(event_count >= 1U);
    assert(events[0].type == EDT_PROTO_UART);
    assert(events[0].value0 == 0xA5U);
}

int main(void)
{
    test_uart_decoder();
    (void)test_ring_buffer();
    printf("All unit tests passed.\n");
    return 0;
}
