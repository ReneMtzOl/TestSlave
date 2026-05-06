#include "uart_protocol.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

#define UART_BAUDRATE 115200

#define UART_TX_PIN 2
#define UART_RX_PIN 3

#define UART_BYTE_TIMEOUT_MS 10

static PIO pio_tx;
static uint sm_tx;
static uint offset_tx;

static PIO pio_rx;
static uint sm_rx;
static uint offset_rx;

bool uart_protocol_is_command_frame(uint8_t command)
{
    switch (command)
    {
    case CMD_RELAY_WRITE:
    case CMD_READ_SWITCH:
    case CMD_READ_ADC:
    case CMD_CHECK_I2C_ADDR:
    case CMD_PWM_WRITE:
        return true;

    default:
        return false;
    }
}

bool uart_protocol_is_response_frame(uint8_t command)
{
    switch (command)
    {
    case RSP_ACK:
    case RSP_SWITCH_STATE:
    case RSP_ADC_VALUE:
    case RSP_I2C_PRESENCE:
    case RSP_ERROR:
        return true;

    default:
        return false;
    }
}

static uint8_t calculate_crc(uint8_t command, uint8_t id, uint16_t value)
{
    uint8_t value_low = (uint8_t)(value & 0xFF);
    uint8_t value_high = (uint8_t)((value >> 8) & 0xFF);

    return command ^ id ^ value_low ^ value_high;
}

static void uart_tx_program_putc(PIO pio, uint sm, uint8_t byte)
{
    pio_sm_put_blocking(pio, sm, (uint32_t)byte);
}

static void uart_tx_program_init_custom(
    PIO pio,
    uint sm,
    uint offset,
    uint pin_tx,
    uint baudrate)
{
    pio_sm_set_pins_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
    pio_sm_set_pindirs_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
    pio_gpio_init(pio, pin_tx);

    pio_sm_config config = uart_tx_program_get_default_config(offset);

    // UART sends LSB first.
    // Autopull is disabled because each byte is manually written to the TX FIFO.
    sm_config_set_out_shift(&config, true, false, 32);

    // OUT and side-set use the same TX pin.
    sm_config_set_out_pins(&config, pin_tx, 1);
    sm_config_set_sideset_pins(&config, pin_tx);

    // Join FIFOs to get a deeper TX FIFO.
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    // The PIO UART program transmits 1 bit every 8 PIO cycles.
    float clock_divider =
        (float)clock_get_hz(clk_sys) / (8.0f * (float)baudrate);

    sm_config_set_clkdiv(&config, clock_divider);

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_set_enabled(pio, sm, true);
}

static void uart_rx_program_init_custom(
    PIO pio,
    uint sm,
    uint offset,
    uint pin_rx,
    uint baudrate)
{
    pio_gpio_init(pio, pin_rx);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_rx, 1, false);

    // UART idle state is HIGH.
    gpio_pull_up(pin_rx);

    pio_sm_config config = uart_rx_program_get_default_config(offset);

    // Used by WAIT and IN instructions.
    sm_config_set_in_pins(&config, pin_rx);

    // Used by JMP PIN instruction to verify the stop bit.
    sm_config_set_jmp_pin(&config, pin_rx);

    // The RX PIO program uses a manual PUSH instruction.
    // Therefore autopush must remain disabled.
    sm_config_set_in_shift(&config, true, false, 32);

    // Join FIFOs to get a deeper RX FIFO.
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);

    // The PIO UART program samples 1 bit every 8 PIO cycles.
    float clock_divider =
        (float)clock_get_hz(clk_sys) / (8.0f * (float)baudrate);

    sm_config_set_clkdiv(&config, clock_divider);

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_set_enabled(pio, sm, true);
}

static uint8_t uart_rx_program_getc(PIO pio, uint sm)
{
    // With right shift enabled and manual PUSH after 8 bits, the received byte
    // is stored in the upper byte of the RX FIFO word.
    io_rw_8 *rx_fifo_msb = (io_rw_8 *)&pio->rxf[sm] + 3;

    while (pio_sm_is_rx_fifo_empty(pio, sm))
    {
        tight_loop_contents();
    }

    return (uint8_t)(*rx_fifo_msb);
}

static bool uart_rx_get_byte_timeout(uint8_t *byte, uint32_t timeout_ms)
{
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (!time_reached(deadline))
    {
        if (!pio_sm_is_rx_fifo_empty(pio_rx, sm_rx))
        {
            *byte = uart_rx_program_getc(pio_rx, sm_rx);
            return true;
        }

        tight_loop_contents();
    }

    return false;
}

void uart_protocol_init(void)
{

    printf("Initializing PIO UART TX on GPIO %u...\r\n", UART_TX_PIN);

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &uart_tx_program,
            &pio_tx,
            &sm_tx,
            &offset_tx,
            UART_TX_PIN,
            1,
            true))
    {
        panic("Failed to allocate PIO resources for UART TX");
    }

    uart_tx_program_init_custom(
        pio_tx,
        sm_tx,
        offset_tx,
        UART_TX_PIN,
        UART_BAUDRATE);

    printf("PIO UART TX initialized\r\n");

    printf("Initializing PIO UART RX on GPIO %u...\r\n", UART_RX_PIN);

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &uart_rx_program,
            &pio_rx,
            &sm_rx,
            &offset_rx,
            UART_RX_PIN,
            1,
            true))
    {
        panic("Failed to allocate PIO resources for UART RX");
    }

    uart_rx_program_init_custom(
        pio_rx,
        sm_rx,
        offset_rx,
        UART_RX_PIN,
        UART_BAUDRATE);

    printf("PIO UART RX initialized\r\n");
}

void uart_protocol_send_frame(uint8_t command, uint8_t id, uint16_t value)
{
    uint8_t value_low = (uint8_t)(value & 0xFF);
    uint8_t value_high = (uint8_t)((value >> 8) & 0xFF);
    uint8_t crc = calculate_crc(command, id, value);

    uart_tx_program_putc(pio_tx, sm_tx, UART_FRAME_SOF);
    uart_tx_program_putc(pio_tx, sm_tx, command);
    uart_tx_program_putc(pio_tx, sm_tx, id);
    uart_tx_program_putc(pio_tx, sm_tx, value_low);
    uart_tx_program_putc(pio_tx, sm_tx, value_high);
    uart_tx_program_putc(pio_tx, sm_tx, crc);
}

bool uart_protocol_receive_frame_timeout(
    uart_frame_t *frame,
    uint32_t timeout_ms)
{
    absolute_time_t frame_deadline = make_timeout_time_ms(timeout_ms);

    while (!time_reached(frame_deadline))
    {
        uint8_t sof = 0;

        if (!uart_rx_get_byte_timeout(&sof, 1))
        {
            continue;
        }

        if (sof != UART_FRAME_SOF)
        {
            continue;
        }

        uint8_t command = 0;
        uint8_t id = 0;
        uint8_t value_low = 0;
        uint8_t value_high = 0;
        uint8_t crc = 0;

        if (!uart_rx_get_byte_timeout(&command, UART_BYTE_TIMEOUT_MS))
        {
            printf("UART RX ERROR: timeout waiting CMD\r\n");
            return false;
        }

        if (!uart_rx_get_byte_timeout(&id, UART_BYTE_TIMEOUT_MS))
        {
            printf("UART RX ERROR: timeout waiting ID\r\n");
            return false;
        }

        if (!uart_rx_get_byte_timeout(&value_low, UART_BYTE_TIMEOUT_MS))
        {
            printf("UART RX ERROR: timeout waiting VALUE_LOW\r\n");
            return false;
        }

        if (!uart_rx_get_byte_timeout(&value_high, UART_BYTE_TIMEOUT_MS))
        {
            printf("UART RX ERROR: timeout waiting VALUE_HIGH\r\n");
            return false;
        }

        if (!uart_rx_get_byte_timeout(&crc, UART_BYTE_TIMEOUT_MS))
        {
            printf("UART RX ERROR: timeout waiting CRC\r\n");
            return false;
        }

        uint16_t value = (uint16_t)value_low |
                         ((uint16_t)value_high << 8);

        uint8_t expected_crc = calculate_crc(command, id, value);

        if (crc != expected_crc)
        {
            printf(
                "UART CRC ERROR: CMD=0x%02X ID=%u VALUE=%u RX_CRC=0x%02X EXP_CRC=0x%02X\r\n",
                command,
                id,
                value,
                crc,
                expected_crc);

            return false;
        }

        frame->command = command;
        frame->id = id;
        frame->value = value;

        printf("UART RX FRAME OK: CMD=0x%02X ID=%u VALUE=%u\r\n",
               command,
               id,
               value);

        return true;
    }

    return false;
}

void uart_protocol_flush_rx(void)
{
    while (!pio_sm_is_rx_fifo_empty(pio_rx, sm_rx))
    {
        (void)uart_rx_program_getc(pio_rx, sm_rx);
    }
}
