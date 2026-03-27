#ifndef EDT_PROTOCOL_DECODER_H
#define EDT_PROTOCOL_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "logic_analyzer.h"

typedef enum {
    EDT_PROTO_UART = 0,
    EDT_PROTO_I2C,
    EDT_PROTO_SPI
} edt_proto_type_t;

typedef struct {
    edt_proto_type_t type;
    uint64_t timestamp_ns;
    uint32_t value0;
    uint32_t value1;
    char summary[96];
} edt_proto_event_t;

typedef struct {
    uint8_t rx_channel;
    uint32_t baudrate;
    uint32_t sample_rate_hz;
} edt_uart_decoder_config_t;

typedef struct {
    uint8_t scl_channel;
    uint8_t sda_channel;
} edt_i2c_decoder_config_t;

typedef struct {
    uint8_t clk_channel;
    uint8_t cs_channel;
    uint8_t mosi_channel;
    uint8_t miso_channel;
    uint8_t cpol;
    uint8_t cpha;
} edt_spi_decoder_config_t;

size_t edt_decode_uart(const edt_logic_sample_t *samples,
                       size_t sample_count,
                       const edt_uart_decoder_config_t *cfg,
                       edt_proto_event_t *events,
                       size_t max_events);

size_t edt_decode_i2c(const edt_logic_sample_t *samples,
                      size_t sample_count,
                      const edt_i2c_decoder_config_t *cfg,
                      edt_proto_event_t *events,
                      size_t max_events);

size_t edt_decode_spi(const edt_logic_sample_t *samples,
                      size_t sample_count,
                      const edt_spi_decoder_config_t *cfg,
                      edt_proto_event_t *events,
                      size_t max_events);

#endif
