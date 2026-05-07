#include "slave_app.h"

#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#include "board_defs.h"
#include "uart_protocol.h"

#define SLAVE_FRAME_TIMEOUT_MS 100

// Maps a logical relay ID received over UART to the actual relay GPIO.
static int get_relay_gpio(uint8_t relay_id);

// Maps a logical switch ID received over UART to the actual switch input GPIO.
static int get_switch_gpio(uint8_t switch_id);

// Maps a logical PWM ID received over UART to the actual PWM output GPIO.
static int get_pwm_gpio(uint8_t pwm_id);

// Sends a generic ACK response to the master for the specified command ID.
static void send_ack(uint8_t id);

// Sends a generic error response to the master with an application-level error code.
static void send_error(uint8_t id, uint16_t error_code);

// Checks whether an I2C device responds at the requested 7-bit address.
static void handle_check_i2c_address(uint8_t address);

// Reads one of the slave ADC channels and sends the result back to the master.
static void handle_read_adc(uint8_t adc_id);

// Sets a relay GPIO according to the value received from the master.
static void handle_relay_write(uint8_t relay_id, uint16_t value);

// Reads a switch GPIO and sends its logic state back to the master.
static void handle_read_switch(uint8_t switch_id);

// Configures a relay pin as a safe low-state digital output.
static void setup_slave_relay_pin(uint pin);

// Configures a switch pin as a digital input with a pull-down for deterministic tests.
static void setup_slave_switch_pin(uint pin);

// Configures a GPIO as a PWM output and starts it at 0% duty cycle.
static void setup_slave_pwm_pin(uint pin);

// Sets a slave PWM output to the requested duty cycle percentage.
static void handle_pwm_write(uint8_t pwm_id, uint16_t duty_percent);

static int get_relay_gpio(uint8_t relay_id)
{
    switch (relay_id)
    {
    case SLAVE_REL1:
        return REL1_GPIO;
    case SLAVE_REL2:
        return REL2_GPIO;
    case SLAVE_REL3:
        return REL3_GPIO;
    case SLAVE_REL4:
        return REL4_GPIO;
    default:
        return -1;
    }
}

static int get_switch_gpio(uint8_t switch_id)
{
    switch (switch_id)
    {
    case SLAVE_SW1_U:
        return SW1_U_GPIO;
    case SLAVE_SW1_D:
        return SW1_D_GPIO;
    case SLAVE_SW2_U:
        return SW2_U_GPIO;
    case SLAVE_SW2_D:
        return SW2_D_GPIO;
    case SLAVE_SW3_U:
        return SW3_U_GPIO;
    case SLAVE_SW3_D:
        return SW3_D_GPIO;
    case SLAVE_SW4_U:
        return SW4_U_GPIO;
    case SLAVE_SW4_D:
        return SW4_D_GPIO;
    case SLAVE_DOOR1_D:
        return SW_DOOR1_GPIO;
    case SLAVE_DOOR2_D:
        return SW_DOOR2_GPIO;
    default:
        return -1;
    }
}

static int get_pwm_gpio(uint8_t pwm_id)
{
    switch (pwm_id)
    {
    case SLAVE_PWM1:
        return PWM1_GPIO;
    case SLAVE_PWM2:
        return PWM2_GPIO;
    default:
        return -1;
    }
}

static void send_ack(uint8_t id)
{
    uart_protocol_send_frame(RSP_ACK, id, 0);
}

static void send_error(uint8_t id, uint16_t error_code)
{
    uart_protocol_send_frame(RSP_ERROR, id, error_code);
}

static void handle_check_i2c_address(uint8_t address)
{
    // En el SDK de Raspberry Pi Pico, una escritura de 0 bytes puede retornar 0 sin generar tráfico.
    // La forma estándar y confiable de hacer un "probe" es intentar leer 1 byte.
    uint8_t rxdata;
    int result = i2c_read_blocking(i2c0, address, &rxdata, 1, false);
    bool online = result >= 0;

    printf("I2C address 0x%02X presence=%u\r\n", address, online ? 1 : 0);
    uart_protocol_send_frame(RSP_I2C_PRESENCE, address, online ? 1 : 0);
}

static void handle_read_adc(uint8_t adc_id)
{
    uint16_t adc_value = 0;

    if (adc_id == 0)
    {
        adc_select_input(0);
    }
    else if (adc_id == 1)
    {
        adc_select_input(1);
    }
    else if (adc_id == 2)
    {
        adc_select_input(2);
    }
    else
    {
        printf("ERROR: Invalid ADC ID %u\r\n", adc_id);
        send_error(adc_id, 3);
        return;
    }

    adc_value = adc_read();
    printf("ADC ID %u value=%u\r\n", adc_id, adc_value);
    uart_protocol_send_frame(RSP_ADC_VALUE, adc_id, adc_value);
}

static void handle_relay_write(uint8_t relay_id, uint16_t value)
{
    int relay_gpio = get_relay_gpio(relay_id);

    if (relay_gpio < 0)
    {
        printf("ERROR: Invalid relay ID %u\r\n", relay_id);
        send_error(relay_id, 1);
        return;
    }

    gpio_put((uint)relay_gpio, value ? 1 : 0);

    printf("Relay ID %u set to %u\r\n", relay_id, value ? 1 : 0);

    send_ack(relay_id);
}

static void handle_read_switch(uint8_t switch_id)
{
    int switch_gpio = get_switch_gpio(switch_id);

    if (switch_gpio < 0)
    {
        printf("ERROR: Invalid switch ID %u\r\n", switch_id);
        send_error(switch_id, 2);
        return;
    }

    uint16_t state = gpio_get((uint)switch_gpio) ? 1 : 0;

    printf("Switch ID %u state = %u\r\n", switch_id, state);

    uart_protocol_send_frame(RSP_SWITCH_STATE, switch_id, state);
}

static void setup_slave_relay_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_disable_pulls(pin);
    gpio_put(pin, 0);
}

static void setup_slave_switch_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_down(pin);
}

static void setup_slave_pwm_pin(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    // The wrap value creates a simple 0..1000 duty scale.
    // A value of 300 means 30%, 850 means 85%, etc.
    pwm_set_wrap(slice_num, 1000);
    pwm_set_chan_level(slice_num, channel, 0);
    pwm_set_enabled(slice_num, true);
}

static void handle_pwm_write(uint8_t pwm_id, uint16_t duty_percent)
{
    int pwm_gpio = get_pwm_gpio(pwm_id);

    if (pwm_gpio < 0)
    {
        printf("ERROR: Invalid PWM ID %u\r\n", pwm_id);
        send_error(pwm_id, 4);
        return;
    }

    if (duty_percent > 100)
    {
        printf("ERROR: Invalid PWM duty %u for PWM ID %u\r\n", duty_percent, pwm_id);
        send_error(pwm_id, 5);
        return;
    }

    uint slice_num = pwm_gpio_to_slice_num((uint)pwm_gpio);
    uint channel = pwm_gpio_to_channel((uint)pwm_gpio);
    uint16_t level = (uint16_t)((duty_percent * 1000u) / 100u);

    pwm_set_chan_level(slice_num, channel, level);

    printf("PWM ID %u on GPIO %d set to %u%%\r\n", pwm_id, pwm_gpio, duty_percent);
    send_ack(pwm_id);
}

void slave_app_init(void)
{
    printf("Initializing slave UART protocol...\r\n");

    uart_protocol_init();
    uart_protocol_flush_rx();

    printf("Slave UART protocol initialized\r\n");

    setup_slave_relay_pin(REL1_GPIO);
    setup_slave_relay_pin(REL2_GPIO);
    setup_slave_relay_pin(REL3_GPIO);
    setup_slave_relay_pin(REL4_GPIO);

    setup_slave_switch_pin(SW1_U_GPIO);
    setup_slave_switch_pin(SW1_D_GPIO);
    setup_slave_switch_pin(SW2_U_GPIO);
    setup_slave_switch_pin(SW2_D_GPIO);
    setup_slave_switch_pin(SW3_U_GPIO);
    setup_slave_switch_pin(SW3_D_GPIO);
    setup_slave_switch_pin(SW4_U_GPIO);
    setup_slave_switch_pin(SW4_D_GPIO);
    setup_slave_switch_pin(SW_DOOR1_GPIO);
    setup_slave_switch_pin(SW_DOOR2_GPIO);

    gpio_set_function((uint)I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function((uint)I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);
    i2c_init(i2c0, 100 * 1000);

    adc_init();
    adc_gpio_init(ADC0_GPIO);
    adc_gpio_init(ADC1_GPIO);
    adc_gpio_init(ADC2_GPIO);

    setup_slave_pwm_pin(PWM1_GPIO);
    setup_slave_pwm_pin(PWM2_GPIO);

    printf("Slave GPIO pins initialized\r\n");
    printf("Slave ready to receive commands\r\n");
}

void slave_app_task(void)
{
    uart_frame_t frame;

    if (!uart_protocol_receive_frame_timeout(&frame, SLAVE_FRAME_TIMEOUT_MS))
    {
        return;
    }

    printf("RX frame: CMD=0x%02X, ID=%u, VALUE=%u\r\n",
           frame.command,
           frame.id,
           frame.value);

    switch (frame.command)
    {
    case CMD_RELAY_WRITE:
        handle_relay_write(frame.id, frame.value);
        break;

    case CMD_READ_SWITCH:
        printf("Command: READ SWITCH\r\n");
        handle_read_switch(frame.id);
        break;

    case CMD_CHECK_I2C_ADDR:
        printf("Command: CHECK I2C ADDR 0x%02X\r\n", frame.id);
        handle_check_i2c_address(frame.id);
        break;

    case CMD_READ_ADC:
        printf("Command: READ ADC %u\r\n", frame.id);
        handle_read_adc(frame.id);
        break;

    case CMD_PWM_WRITE:
        printf("Command: PWM WRITE ID=%u DUTY=%u%%\r\n", frame.id, frame.value);
        handle_pwm_write(frame.id, frame.value);
        break;

    default:
        if (frame.command == RSP_ACK ||
            frame.command == RSP_SWITCH_STATE ||
            frame.command == RSP_ADC_VALUE ||
            frame.command == RSP_I2C_PRESENCE ||
            frame.command == RSP_ERROR)
        {
            // Ignore response frames that may be seen because of the current UART echo behavior.
            break;
        }

        printf("Unknown command: 0x%02X\r\n", frame.command);
        send_error(frame.id, 0xFFFF);
        break;
    }
}
