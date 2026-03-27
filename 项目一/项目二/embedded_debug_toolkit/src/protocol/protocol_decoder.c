#include "protocol_decoder.h"

#include <stdio.h>
#include <string.h>

static inline uint8_t edt_bit(uint8_t value, uint8_t channel)
{
    return (uint8_t)((value >> channel) & 0x1U);
}

size_t edt_decode_uart(const edt_logic_sample_t *samples,
                       size_t sample_count,
                       const edt_uart_decoder_config_t *cfg,
                       edt_proto_event_t *events,
                       size_t max_events)
{
    size_t event_count = 0U;
    size_t i;
    uint32_t samples_per_bit;

    if (!samples || !cfg || !events || max_events == 0U || cfg->baudrate == 0U || cfg->sample_rate_hz == 0U) {
        return 0U;
    }
    if (cfg->rx_channel >= 8U) {
        return 0U;
    }

    samples_per_bit = cfg->sample_rate_hz / cfg->baudrate;
    if (samples_per_bit < 3U) {
        return 0U;
    }

    i = 1U;
    while (i < sample_count && event_count < max_events) {
        const uint8_t prev = edt_bit(samples[i - 1U].channels, cfg->rx_channel);
        const uint8_t curr = edt_bit(samples[i].channels, cfg->rx_channel);

        if (prev == 1U && curr == 0U) {
            const size_t start_index = i;
            size_t sample_pos = start_index + samples_per_bit + (samples_per_bit / 2U);
            size_t bit_index;
            uint8_t value = 0U;

            if (start_index + (size_t)samples_per_bit / 2U >= sample_count) {
                break;
            }
            if (edt_bit(samples[start_index + (size_t)samples_per_bit / 2U].channels, cfg->rx_channel) != 0U) {
                i++;
                continue;
            }

            for (bit_index = 0U; bit_index < 8U; bit_index++) {
                if (sample_pos >= sample_count) {
                    break;
                }
                value |= (uint8_t)(edt_bit(samples[sample_pos].channels, cfg->rx_channel) << bit_index);
                sample_pos += samples_per_bit;
            }
            if (bit_index < 8U || sample_pos >= sample_count) {
                break;
            }

            if (edt_bit(samples[sample_pos].channels, cfg->rx_channel) == 1U) {
                edt_proto_event_t *evt = &events[event_count++];
                evt->type = EDT_PROTO_UART;
                evt->timestamp_ns = samples[start_index].timestamp_ns;
                evt->value0 = value;
                evt->value1 = 0U;
                (void)snprintf(evt->summary, sizeof(evt->summary), "UART RX: 0x%02X (%u)", value, value);
            }
            i = sample_pos + 1U;
            continue;
        }
        i++;
    }

    return event_count;
}

size_t edt_decode_i2c(const edt_logic_sample_t *samples,
                      size_t sample_count,
                      const edt_i2c_decoder_config_t *cfg,
                      edt_proto_event_t *events,
                      size_t max_events)
{
    size_t event_count = 0U;
    size_t i;
    int active = 0;
    uint8_t bit_count = 0U;
    uint8_t current = 0U;
    uint8_t expect_ack = 0U;

    if (!samples || !cfg || !events || max_events == 0U) {
        return 0U;
    }
    if (cfg->scl_channel >= 8U || cfg->sda_channel >= 8U) {
        return 0U;
    }

    for (i = 1U; i < sample_count && event_count < max_events; i++) {
        const uint8_t prev_scl = edt_bit(samples[i - 1U].channels, cfg->scl_channel);
        const uint8_t prev_sda = edt_bit(samples[i - 1U].channels, cfg->sda_channel);
        const uint8_t scl = edt_bit(samples[i].channels, cfg->scl_channel);
        const uint8_t sda = edt_bit(samples[i].channels, cfg->sda_channel);

        if (prev_sda == 1U && sda == 0U && scl == 1U) {
            active = 1;
            bit_count = 0U;
            current = 0U;
            expect_ack = 0U;
            continue;
        }
        if (prev_sda == 0U && sda == 1U && scl == 1U) {
            active = 0;
            continue;
        }
        if (!active) {
            continue;
        }

        if (prev_scl == 0U && scl == 1U) {
            if (!expect_ack) {
                current = (uint8_t)((current << 1U) | sda);
                bit_count++;
                if (bit_count == 8U) {
                    expect_ack = 1U;
                    bit_count = 0U;
                }
            } else {
                edt_proto_event_t *evt = &events[event_count++];
                evt->type = EDT_PROTO_I2C;
                evt->timestamp_ns = samples[i].timestamp_ns;
                evt->value0 = current;
                evt->value1 = (sda == 0U) ? 1U : 0U;
                (void)snprintf(evt->summary,
                               sizeof(evt->summary),
                               "I2C BYTE: 0x%02X ACK:%s",
                               current,
                               (sda == 0U) ? "Y" : "N");
                current = 0U;
                expect_ack = 0U;
                if (event_count >= max_events) {
                    break;
                }
            }
        }
    }

    return event_count;
}

size_t edt_decode_spi(const edt_logic_sample_t *samples,
                      size_t sample_count,
                      const edt_spi_decoder_config_t *cfg,
                      edt_proto_event_t *events,
                      size_t max_events)
{
    size_t event_count = 0U;
    size_t i;
    uint8_t mosi_byte = 0U;
    uint8_t miso_byte = 0U;
    uint8_t bits = 0U;
    int active = 0;
    const uint8_t idle = cfg ? (cfg->cpol ? 1U : 0U) : 0U;
    const uint8_t sample_on_rising = (cfg && ((cfg->cpha == 0U && cfg->cpol == 0U) ||
                                              (cfg->cpha == 1U && cfg->cpol == 1U)))
                                         ? 1U
                                         : 0U;

    if (!samples || !cfg || !events || max_events == 0U) {
        return 0U;
    }
    if (cfg->clk_channel >= 8U || cfg->cs_channel >= 8U || cfg->mosi_channel >= 8U ||
        cfg->miso_channel >= 8U) {
        return 0U;
    }

    for (i = 1U; i < sample_count && event_count < max_events; i++) {
        const uint8_t prev_clk = edt_bit(samples[i - 1U].channels, cfg->clk_channel);
        const uint8_t clk = edt_bit(samples[i].channels, cfg->clk_channel);
        const uint8_t cs = edt_bit(samples[i].channels, cfg->cs_channel);
        int sample_edge = 0;

        if (cs == 1U) {
            active = 0;
            bits = 0U;
            continue;
        }
        if (!active) {
            active = 1;
            bits = 0U;
            mosi_byte = 0U;
            miso_byte = 0U;
        }

        if (sample_on_rising) {
            sample_edge = (prev_clk == 0U && clk == 1U);
        } else {
            sample_edge = (prev_clk == 1U && clk == 0U);
        }

        if (!sample_edge) {
            continue;
        }

        if (prev_clk == idle && bits == 0U && cfg->cpha == 1U) {
            continue;
        }

        mosi_byte = (uint8_t)((mosi_byte << 1U) | edt_bit(samples[i].channels, cfg->mosi_channel));
        miso_byte = (uint8_t)((miso_byte << 1U) | edt_bit(samples[i].channels, cfg->miso_channel));
        bits++;
        if (bits == 8U) {
            edt_proto_event_t *evt = &events[event_count++];
            evt->type = EDT_PROTO_SPI;
            evt->timestamp_ns = samples[i].timestamp_ns;
            evt->value0 = mosi_byte;
            evt->value1 = miso_byte;
            (void)snprintf(evt->summary,
                           sizeof(evt->summary),
                           "SPI MOSI:0x%02X MISO:0x%02X",
                           mosi_byte,
                           miso_byte);
            bits = 0U;
            mosi_byte = 0U;
            miso_byte = 0U;
        }
    }

    return event_count;
}
