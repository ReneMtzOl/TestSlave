#include "master_test.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "board_defs.h"
#include "uart_protocol.h"
#include "hardware/adc.h"

#define RESPONSE_TIMEOUT_MS 100
#define RELAY_STEP_DELAY_MS 500
#define SWITCH_SETTLING_TIME_MS 20

static bool pwm_write(uint8_t pwm_id, uint8_t duty_percent);
static bool run_pwm_adc_test(void);

static bool relay_write(uint8_t relay_id, bool state);
static bool run_relay_full_test(void);

static bool wait_ack(uint8_t expected_id)
{
    uart_frame_t response;
    absolute_time_t deadline = make_timeout_time_ms(RESPONSE_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        if (!uart_protocol_receive_frame_timeout(&response, 20))
        {
            continue;
        }

        /*printf("Received ACK candidate: command=0x%02X, id=%u, value=%u\r\n",
               response.command,
               response.id,
               response.value);
*/
        // Ignore echoed command frames.
        if (response.command == CMD_RELAY_WRITE ||
            response.command == CMD_READ_SWITCH ||
            response.command == CMD_READ_ADC ||
            response.command == CMD_CHECK_I2C_ADDR ||
            response.command == CMD_PWM_WRITE)
        {
            // printf("Ignoring echoed command 0x%02X\r\n", response.command);
            continue;
        }

        if (response.command == RSP_ERROR)
        {
            printf("ERROR: Slave returned error. ID=%u, code=%u\r\n",
                   response.id,
                   response.value);
            return false;
        }

        if (response.command != RSP_ACK)
        {
            printf("ERROR: Expected ACK 0x%02X, got 0x%02X\r\n",
                   RSP_ACK,
                   response.command);
            continue;
        }

        if (response.id != expected_id)
        {
            printf("ERROR: ACK ID mismatch. Expected %u, got %u\r\n",
                   expected_id,
                   response.id);
            continue;
        }

        printf("ACK OK for ID %u\r\n", expected_id);
        return true;
    }

    printf("ERROR: Timeout waiting valid ACK for ID %u\r\n", expected_id);
    return false;
}

static bool wait_i2c_presence(uint8_t address, bool *online)
{
    uart_frame_t response;
    absolute_time_t deadline = make_timeout_time_ms(RESPONSE_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        if (!uart_protocol_receive_frame_timeout(&response, 20))
        {
            continue;
        }

        printf("Received I2C response candidate: command=0x%02X, id=%u, value=%u\r\n",
               response.command,
               response.id,
               response.value);

        if (response.command == CMD_RELAY_WRITE ||
            response.command == CMD_READ_SWITCH ||
            response.command == CMD_READ_ADC ||
            response.command == CMD_CHECK_I2C_ADDR)
        {
            // printf("Ignoring echoed command 0x%02X\r\n", response.command);
            continue;
        }

        if (response.command == RSP_ERROR)
        {
            printf("ERROR: Slave returned error. ID=%u, code=%u\r\n",
                   response.id,
                   response.value);
            return false;
        }

        if (response.command != RSP_I2C_PRESENCE)
        {
            printf("ERROR: Expected I2C presence response 0x%02X, got 0x%02X\r\n",
                   RSP_I2C_PRESENCE,
                   response.command);
            continue;
        }

        if (response.id != address)
        {
            printf("ERROR: I2C address mismatch. Expected 0x%02X, got 0x%02X\r\n",
                   address,
                   response.id);
            continue;
        }

        *online = response.value ? true : false;
        printf("I2C address 0x%02X online=%u\r\n", address, *online ? 1 : 0);
        return true;
    }

    printf("ERROR: Timeout waiting valid I2C presence response for address 0x%02X\r\n", address);
    return false;
}

static bool run_relay_test(void)
{
    printf("Starting relay test\r\n");

    for (int r = SLAVE_REL1; r <= SLAVE_REL4; r++)
    {
        if (!relay_write(r, true))
            return false;
        sleep_ms(300);

        if (!relay_write(r, false))
            return false;
        sleep_ms(300);
    }

    printf("Relay test OK\r\n");

    return true;
}

static bool read_slave_switch(uint8_t switch_id, bool *state)
{
    uart_frame_t response;

    printf("Sending CMD_READ_SWITCH for ID %u\r\n", switch_id);
    uart_protocol_send_frame(CMD_READ_SWITCH, switch_id, 0);

    absolute_time_t deadline = make_timeout_time_ms(RESPONSE_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        if (!uart_protocol_receive_frame_timeout(&response, 20))
        {
            continue;
        }

        printf("Received response: command=0x%02X, id=%u, value=%u\r\n",
               response.command,
               response.id,
               response.value);

        // Ignore echoed command frames.
        if (response.command == CMD_READ_SWITCH ||
            response.command == CMD_RELAY_WRITE)
        {
            printf("Ignoring echoed command 0x%02X\r\n", response.command);
            continue;
        }

        if (response.command == RSP_ERROR)
        {
            printf("ERROR: Slave returned error. ID=%u, code=%u\r\n",
                   response.id,
                   response.value);
            return false;
        }

        if (response.command != RSP_SWITCH_STATE)
        {
            printf("ERROR: Expected switch state 0x%02X, got 0x%02X\r\n",
                   RSP_SWITCH_STATE,
                   response.command);
            continue;
        }

        if (response.id != switch_id)
        {
            printf("ERROR: Switch ID mismatch. Expected %u, got %u\r\n",
                   switch_id,
                   response.id);
            continue;
        }

        *state = response.value ? true : false;

        printf("Switch ID %u state: %s\r\n",
               switch_id,
               *state ? "HIGH" : "LOW");

        return true;
    }

    printf("ERROR: Timeout waiting valid switch response for ID %u\r\n", switch_id);
    return false;
}

static bool test_switch(uint8_t switch_id)
{
    int pin = -1;

    switch (switch_id)
    {
    case SLAVE_SW1_U:
        pin = SW1_U_GPIO;
        break;
    case SLAVE_SW1_D:
        pin = SW1_D_GPIO;
        break;
    case SLAVE_SW2_U:
        pin = SW2_U_GPIO;
        break;
    case SLAVE_SW2_D:
        pin = SW2_D_GPIO;
        break;
    case SLAVE_SW3_U:
        pin = SW3_U_GPIO;
        break;
    case SLAVE_SW3_D:
        pin = SW3_D_GPIO;
        break;
    case SLAVE_SW4_U:
        pin = SW4_U_GPIO;
        break;
    case SLAVE_SW4_D:
        pin = SW4_D_GPIO;
        break;
    case SLAVE_DOOR1_D:
        pin = SW_DOOR1_GPIO;
        break;

    case SLAVE_DOOR2_D:
        pin = SW_DOOR2_GPIO;
        break;
    default:
        return false;
    }

    bool state = false;

    printf("Testing switch ID %u LOW/HIGH\r\n", switch_id);

    gpio_put(pin, 0);
    sleep_ms(SWITCH_SETTLING_TIME_MS);

    if (!read_slave_switch(switch_id, &state))
    {
        gpio_put(pin, 0);
        return false;
    }
    if (state != false)
    {
        printf("ERROR: Switch ID %u should be LOW but read HIGH\r\n", switch_id);
        gpio_put(pin, 0);
        return false;
    }

    gpio_put(pin, 1);
    sleep_ms(SWITCH_SETTLING_TIME_MS);

    if (!read_slave_switch(switch_id, &state))
    {
        gpio_put(pin, 0);
        return false;
    }

    if (state != true)
    {
        printf("ERROR: Switch ID %u should be HIGH but read LOW\r\n", switch_id);
        gpio_put(pin, 0);
        return false;
    }

    gpio_put(pin, 0);

    printf("Switch ID %u LOW/HIGH OK\r\n", switch_id);

    return true;
}

static void switch_outputs_all_off(void)
{
    gpio_put(SW1_U_GPIO, 0);
    gpio_put(SW1_D_GPIO, 0);
    gpio_put(SW2_U_GPIO, 0);
    gpio_put(SW2_D_GPIO, 0);
    gpio_put(SW3_U_GPIO, 0);
    gpio_put(SW3_D_GPIO, 0);
    gpio_put(SW4_U_GPIO, 0);
    gpio_put(SW4_D_GPIO, 0);
    gpio_put(SW_DOOR1_GPIO, 0);
    gpio_put(SW_DOOR2_GPIO, 0);
}

bool run_switch_test(void)
{
    printf("Starting full switch test\r\n");

    switch_outputs_all_off();
    sleep_ms(500);

    if (!test_switch(SLAVE_SW1_U))
        goto fail;
    if (!test_switch(SLAVE_SW4_U))
        goto fail;
    if (!test_switch(SLAVE_SW4_D))
        goto fail;
    if (!test_switch(SLAVE_DOOR1_D))
        goto fail;
    if (!test_switch(SLAVE_DOOR2_D))
        goto fail;
    if (!test_switch(SLAVE_SW1_D))
        goto fail;
    if (!test_switch(SLAVE_SW2_U))
        goto fail;
    if (!test_switch(SLAVE_SW2_D))
        goto fail;
    if (!test_switch(SLAVE_SW3_U))
        goto fail;
    if (!test_switch(SLAVE_SW3_D))
        goto fail;

    switch_outputs_all_off();

    printf("Full switch test OK\r\n");
    return true;

fail:
    switch_outputs_all_off();
    printf("Full switch test FAILED\r\n");
    return false;
}

static void setup_master_output_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_disable_pulls(pin);
    gpio_put(pin, 0);
}

void master_tests_init(void)
{
    printf("Initializing UART protocol...\r\n");
    uart_protocol_init();
    printf("UART protocol initialized\r\n");

    adc_init();
    adc_gpio_init(ADC0_GPIO);
    adc_gpio_init(ADC1_GPIO);

    // Initialize switch GPIOs as outputs for testing
    setup_master_output_pin(SW1_U_GPIO);
    setup_master_output_pin(SW1_D_GPIO);
    setup_master_output_pin(SW2_U_GPIO);
    setup_master_output_pin(SW2_D_GPIO);
    setup_master_output_pin(SW3_U_GPIO);
    setup_master_output_pin(SW3_D_GPIO);
    setup_master_output_pin(SW4_U_GPIO);
    setup_master_output_pin(SW4_D_GPIO);
    setup_master_output_pin(SW_DOOR1_GPIO);
    setup_master_output_pin(SW_DOOR2_GPIO);

    // Initialize board LED
    setup_master_output_pin(BOARD_LED);

    printf("Master GPIO pins initialized\r\n");
}

static bool relay_write(uint8_t relay_id, bool state)
{
    uart_protocol_send_frame(CMD_RELAY_WRITE, relay_id, state ? 1 : 0);

    return wait_ack(relay_id);
}

static bool test_relay(uint8_t relay_id)
{
    printf("Testing relay %u ON/OFF\r\n", relay_id);

    if (!relay_write(relay_id, true))
    {
        printf("ERROR: Relay %u failed to turn ON\r\n", relay_id);
        return false;
    }

    sleep_ms(BLINK_DELAY_MS);

    if (!relay_write(relay_id, false))
    {
        printf("ERROR: Relay %u failed to turn OFF\r\n", relay_id);
        return false;
    }

    sleep_ms(BLINK_DELAY_MS);

    printf("Relay %u OK\r\n", relay_id);

    return true;
}

static bool test_all_relays_blink(void)
{
    printf("Testing all relays blink x3\r\n");

    for (int i = 0; i < 3; i++)
    {
        printf("Cycle %d\r\n", i + 1);

        // ON todos
        for (int r = SLAVE_REL1; r <= SLAVE_REL4; r++)
        {
            if (!relay_write(r, true))
                return false;
        }

        sleep_ms(BLINK_DELAY_MS);

        // OFF todos
        for (int r = SLAVE_REL1; r <= SLAVE_REL4; r++)
        {
            if (!relay_write(r, false))
                return false;
        }

        sleep_ms(BLINK_DELAY_MS);
    }

    printf("All relays blink OK\r\n");

    return true;
}

static bool run_relay_full_test(void)
{
    printf("Starting full relay test\r\n");

    if (!test_relay(SLAVE_REL1))
        return false;
    if (!test_relay(SLAVE_REL2))
        return false;
    if (!test_relay(SLAVE_REL3))
        return false;
    if (!test_relay(SLAVE_REL4))
        return false;

    if (!test_all_relays_blink())
        return false;

    printf("Full relay test OK\r\n");

    return true;
}

static bool run_i2c_presence_check(void)
{
    const uint8_t address = 0x50;
    bool online = false;

    printf("Requesting I2C address presence check for 0x%02X\r\n", address);
    uart_protocol_send_frame(CMD_CHECK_I2C_ADDR, address, 0);

    if (!wait_i2c_presence(address, &online))
    {
        return false;
    }

    if (!online)
    {
        printf("ERROR: I2C device at 0x%02X did not respond\r\n", address);
        return false;
    }

    printf("I2C device at 0x%02X responded OK\r\n", address);
    return true;
}

static bool wait_adc_value(uint8_t adc_id, uint16_t *value)
{
    uart_frame_t response;
    absolute_time_t deadline = make_timeout_time_ms(RESPONSE_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        if (!uart_protocol_receive_frame_timeout(&response, 20))
        {
            continue;
        }

        printf("Received ADC response candidate: command=0x%02X, id=%u, value=%u\r\n",
               response.command,
               response.id,
               response.value);

        if (response.command == CMD_RELAY_WRITE ||
            response.command == CMD_READ_SWITCH ||
            response.command == CMD_READ_ADC ||
            response.command == CMD_CHECK_I2C_ADDR)
        {
            printf("Ignoring echoed command 0x%02X\r\n", response.command);
            continue;
        }

        if (response.command == RSP_ERROR)
        {
            printf("ERROR: Slave returned error. ID=%u, code=%u\r\n",
                   response.id,
                   response.value);
            return false;
        }

        if (response.command != RSP_ADC_VALUE)
        {
            printf("ERROR: Expected ADC value response 0x%02X, got 0x%02X\r\n",
                   RSP_ADC_VALUE,
                   response.command);
            continue;
        }

        if (response.id != adc_id)
        {
            printf("ERROR: ADC ID mismatch. Expected %u, got %u\r\n",
                   adc_id,
                   response.id);
            continue;
        }

        *value = response.value;
        printf("ADC ID %u value: %u\r\n", adc_id, *value);
        return true;
    }

    printf("ERROR: Timeout waiting valid ADC response for ID %u\r\n", adc_id);
    return false;
}

static bool read_slave_adc(uint8_t adc_id, uint16_t *value)
{
    printf("Sending CMD_READ_ADC for ID %u\r\n", adc_id);
    uart_protocol_send_frame(CMD_READ_ADC, adc_id, 0);

    return wait_adc_value(adc_id, value);
}

static bool run_adc_test(void)
{
    printf("Starting ADC test\r\n");

    uint16_t adc0_value = 0;
    uint16_t adc1_value = 0;
    uint16_t adc2_value = 0;

    if (!read_slave_adc(0, &adc0_value))
    {
        printf("ERROR: Failed to read ADC0\r\n");
        return false;
    }

    sleep_ms(100);

    if (!read_slave_adc(1, &adc1_value))
    {
        printf("ERROR: Failed to read ADC1\r\n");
        return false;
    }

    if (!read_slave_adc(2, &adc2_value))
    {
        printf("ERROR: Failed to read ADC2\r\n");
        return false;
    }
    if (adc0_value == 0)
    {
        printf("ERROR: ADC0 value is 0 (out of range)\r\n");
        return false;
    }
    if (adc1_value == 0)
    {
        printf("ERROR: ADC1 value is 0 (out of range)\r\n");
        return false;
    }

    if (adc2_value == 0)
    {
        printf("ERROR: ADC2 value is 0 (out of range)\r\n");
        return false;
    }

    printf("ADC test OK: ADC0=%u, ADC1=%u, ADC2=%u\r\n", adc0_value, adc1_value, adc2_value);
    return true;
}

bool master_tests_run_all(void)
{
    bool result = true;

    result &= run_switch_test();
    result &= run_relay_full_test();
    result &= run_i2c_presence_check();
    result &= run_adc_test();
    result &= run_pwm_adc_test();
    
    if (result)
    {
        printf("FULL TEST PASSED\r\n");
    }
    else
    {
        printf("FULL TEST FAILED\r\n");
    }

    return result;
}

static bool pwm_write(uint8_t pwm_id, uint8_t duty_percent)
{
    printf("Setting PWM ID %u to %u%%\r\n", pwm_id, duty_percent);

    uart_protocol_send_frame(CMD_PWM_WRITE, pwm_id, duty_percent);

    return wait_ack(pwm_id);
}

static uint16_t read_master_adc_once(uint8_t adc_id)
{
    adc_select_input(adc_id);
    sleep_us(10);
    return adc_read();
}

static uint16_t read_master_adc_average(uint8_t adc_id, uint8_t samples)
{
    uint32_t accumulator = 0;

    for (uint8_t i = 0; i < samples; i++)
    {
        accumulator += read_master_adc_once(adc_id);
        sleep_ms(2);
    }

    return (uint16_t)(accumulator / samples);
}

static bool validate_adc_range(const char *name, uint16_t value, uint16_t min_value, uint16_t max_value)
{
    printf("%s ADC value=%u, expected range=%u..%u\r\n",
           name,
           value,
           min_value,
           max_value);

    if (value < min_value || value > max_value)
    {
        printf("ERROR: %s ADC value out of placeholder range\r\n", name);
        return false;
    }

    return true;
}

static bool run_pwm_adc_test(void)
{
    printf("Starting PWM -> master ADC test\r\n");

    if (!pwm_write(SLAVE_PWM1, PWM1_DUTY_PERCENT))
    {
        printf("ERROR: Failed to set PWM1\r\n");
        return false;
    }

    if (!pwm_write(SLAVE_PWM2, PWM2_DUTY_PERCENT))
    {
        printf("ERROR: Failed to set PWM2\r\n");
        return false;
    }

    sleep_ms(500);

    uint16_t pwm1_adc_value = read_master_adc_average(MASTER_PWM_ADC0_ID, 16);
    uint16_t pwm2_adc_value = read_master_adc_average(MASTER_PWM_ADC1_ID, 16);

    if (!validate_adc_range("PWM1 on master ADC0", pwm1_adc_value,
                            PWM1_ADC_MIN_PLACEHOLDER,
                            PWM1_ADC_MAX_PLACEHOLDER))
    {
        return false;
    }

    if (!validate_adc_range("PWM2 on master ADC1", pwm2_adc_value,
                            PWM2_ADC_MIN_PLACEHOLDER,
                            PWM2_ADC_MAX_PLACEHOLDER))
    {
        return false;
    }

    printf("PWM ADC test OK: PWM1_ADC0=%u, PWM2_ADC1=%u\r\n",
           pwm1_adc_value,
           pwm2_adc_value);

    return true;
}