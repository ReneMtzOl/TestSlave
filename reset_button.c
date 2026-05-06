#include "reset_button.h"

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#define RESET_BUTTON_GPIO 6

static void reset_button_irq_callback(uint gpio, uint32_t events)
{
    if (gpio == RESET_BUTTON_GPIO && (events & GPIO_IRQ_EDGE_FALL))
    {
        watchdog_enable(1, 1);

        while (true)
        {
            tight_loop_contents();
        }
    }
}

void reset_button_init(void)
{
    gpio_init(RESET_BUTTON_GPIO);
    gpio_set_dir(RESET_BUTTON_GPIO, GPIO_IN);
    gpio_disable_pulls(RESET_BUTTON_GPIO);

    gpio_set_irq_enabled_with_callback(
        RESET_BUTTON_GPIO,
        GPIO_IRQ_EDGE_FALL,
        true,
        &reset_button_irq_callback
    );
}