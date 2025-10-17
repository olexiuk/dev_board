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

    const enum led_id_t leds[] = {LED0, LED1, LED2, LED3};
    
    unsigned char val = 0; 

    while(1)
    {
        if (BTN_check_clear_pressed(BTN0))
        {
            if (val < 0b1111)
                val++;
            else
                val = 0;
            for (int i = 0; i < 4; i++)
            {
                if ((val >> i) & 1)
                    LED_set(leds[i], LED_ON);
                else
                    LED_set(leds[i], LED_OFF);
            }
        }
        k_msleep(SLEEP_MS);
    }
    return 0;
}
