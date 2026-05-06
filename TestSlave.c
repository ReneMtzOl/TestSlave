#include <stdio.h>

#include "pico/stdlib.h"
#include "master_test.h"
#include "slave_app.h"
#include "board_defs.h"
#include "reset_button.h"


#define ISMASTER 0
#define USER_BLINK_DELAY_MS 500

int main(void)
{
    stdio_init_all();
    reset_button_init();

    sleep_ms(2000);

    if (ISMASTER)
    {
        sleep_ms(3000);
        printf("Master tester started\r\n");

        master_tests_init();

        bool result = master_tests_run_all();

        if (result)
        {
            printf("All tests passed, blinking LED 3 times\r\n");

            for (int i = 0; i < 3; i++)
            {
                gpio_put(BOARD_LED, 1);
                sleep_ms(USER_BLINK_DELAY_MS);
                gpio_put(BOARD_LED, 0);
                sleep_ms(USER_BLINK_DELAY_MS);
            }
        }
        else
        {
            printf("Tests failed, blinking LED indefinitely\r\n");

            while (true)
            {
                gpio_put(BOARD_LED, 1);
                sleep_ms(USER_BLINK_DELAY_MS);
                gpio_put(BOARD_LED, 0);
                sleep_ms(USER_BLINK_DELAY_MS);
            }
        }

        while (true)
        {
            tight_loop_contents();
        }
    }
    else
    {
        sleep_ms(2000);
        printf("Slave started\r\n");

        slave_app_init();

        while (true)
        {
            slave_app_task();
            tight_loop_contents();
        }
    }
}