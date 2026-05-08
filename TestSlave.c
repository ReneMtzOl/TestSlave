#include <stdio.h>

#include "pico/stdlib.h"
#include "master_test.h"
#include "slave_app.h"
#include "board_defs.h"
#include "reset_button.h"

#define ISMASTER 1
#define USER_BLINK_DELAY_MS 500
#define ERROR_PATTERN_PAUSE_MS 1000

static void board_led_solid_on(void)
{
    gpio_put(BOARD_LED, 1);
}

static void board_led_blink_once(void)
{
    gpio_put(BOARD_LED, 1);
    sleep_ms(USER_BLINK_DELAY_MS);

    gpio_put(BOARD_LED, 0);
    sleep_ms(USER_BLINK_DELAY_MS);
}

static void board_led_error_pattern(uint8_t blink_count)
{
    while (true)
    {
        for (uint8_t i = 0; i < blink_count; i++)
        {
            board_led_blink_once();
        }

        sleep_ms(ERROR_PATTERN_PAUSE_MS);
    }
}

int main(void)
{
    stdio_init_all();
    reset_button_init();

    if (ISMASTER)
    {
        printf("Master tester configuring pins...\r\n");
        master_tests_init();

        sleep_ms(1000);
        printf("Master tester started\r\n");

        master_test_result_t result = master_tests_run_all_with_result();

        if (result == MASTER_TEST_RESULT_OK)
        {
            printf("All tests passed, BOARD_LED solid ON\r\n");

            board_led_solid_on();

            while (true)
            {
                tight_loop_contents();
            }
        }
        else
        {
            printf("Tests failed, BOARD_LED error pattern: %u blinks\r\n",
                   (uint8_t)result);

            board_led_error_pattern((uint8_t)result);
        }
    }
    else
    {
        printf("Slave configuring pins...\r\n");
        slave_app_init();

        sleep_ms(500);
        printf("Slave started\r\n");

        while (true)
        {
            slave_app_task();
            tight_loop_contents();
        }
    }
}
