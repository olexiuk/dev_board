#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)


static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

int main(void)
{
    int ret;
    struct gpio_dt_spec leds[] = {led0, led1, led3, led2};

    for (int i = 0; i < 4; i++)
    {
        if (!gpio_is_ready_dt(leds + i))
            return -1;
        ret = gpio_pin_configure_dt(leds + i, GPIO_OUTPUT_ACTIVE);
        if (ret < 0)
            return ret;
    }

    int current = 0;
    while (1)
    {
        gpio_pin_toggle_dt(leds + current);
        k_msleep(1000);
        if (current == 3)
            current = 0;
        else
            current++;
    }
    return 0;
}