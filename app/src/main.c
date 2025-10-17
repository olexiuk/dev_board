/*
 * main.c
 */

#include <zephyr/kernel.h>
#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 1

int main(void) {
    int ret;

    ret = BTN_init();
    if (ret < 0)
        return ret;

    ret = LED_init();
    if (ret < 0)
        return ret;
    
    while(1)
    {
        if (BTN_check_clear_pressed(BTN0))
        {
            LED_toggle(LED0);
            printk("Button 0 pressed\n");
        }
        k_msleep(SLEEP_MS);
    }
    return 0;
}
