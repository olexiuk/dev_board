/*
 * main.c
 */

#include <zephyr/kernel.h>
#include "BTN.h"
#include "LED.h"

#define SLEEP_MS 10

int main(void) {
    int ret;

    ret = BTN_init();
    if (ret < 0)
        return ret;

    ret = LED_init();
    if (ret < 0)
        return ret;

    const enum led_id_t leds[] = {LED0, LED1, LED2, LED3};
    const enum btn_id_t btns[] = {BTN0, BTN1, BTN2, BTN3};
    
    // each two bits represents a button
    // password can be different lengths
    
    // password is 1233
    // uint16_t password = 0b01101111;
    // uint8_t passwordLength = 4;

    // password is 12332133
    uint16_t password = 0b0110111110011111;
    uint8_t passwordLength = 8;

    uint16_t input = 0;
    bool waiting = false;
    int8_t buttonPressed;

    // used to clear any bits past the length of the password
    uint16_t passwordCap = 0b1111111111111111 >> (16 - (passwordLength * 2));

    LED_set(leds[0], LED_ON);
    
    while(1)
    {
        k_msleep(SLEEP_MS);
        buttonPressed = -1;
        for (int i = 0; i < 4; i++)
        {
            if (BTN_check_clear_pressed(btns[i]))
                buttonPressed = i;
        }
        if (buttonPressed == -1)
            continue;
        if (waiting)
        {
            LED_set(leds[0], LED_ON);
            waiting = false;
            continue;
        }
        if (buttonPressed == 3)
        {
            if (password == input)
            {
                printk("Correct!\n");
                LED_set(leds[0], LED_OFF);
                waiting = true;
            }
            else
            {
                printk("Incorrect!\n");
            }
            input = 0;
            continue;
        }
        // add button pressed to front of input and clear bits that are passed the length of the password
        input = (input << 2 | (buttonPressed + 1)) & passwordCap;
    }
    return 0;
}
