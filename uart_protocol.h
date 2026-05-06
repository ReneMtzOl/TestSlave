#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define UART_FRAME_SOF 0x7E

#define CMD_PWM_WRITE      0x50
#define CMD_RELAY_WRITE     0x10
#define CMD_ALL_RELAYS_OFF  0x11
#define CMD_READ_SWITCH     0x20
#define CMD_READ_ADC        0x30
#define CMD_CHECK_I2C_ADDR  0x40

#define RSP_ACK             0x80
#define RSP_SWITCH_STATE    0x81
#define RSP_ADC_VALUE       0x82
#define RSP_I2C_PRESENCE    0x83
#define RSP_ERROR           0xE0

typedef struct
{
    uint8_t command;
    uint8_t id;
    uint16_t value;
} uart_frame_t;

bool uart_protocol_is_command_frame(uint8_t command);
bool uart_protocol_is_response_frame(uint8_t command);

void uart_protocol_init(void);

void uart_protocol_flush_rx(void);

void uart_protocol_send_frame(
    uint8_t command,
    uint8_t id,
    uint16_t value
);

bool uart_protocol_receive_frame_timeout(
    uart_frame_t *frame,
    uint32_t timeout_ms
);

#endif
