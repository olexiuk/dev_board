#include <zephyr/smf.h>

#include "BTN.h"
#include "LED.h"
#include "my_state_machine.h"

const enum led_id_t leds[] = {LED0, LED1, LED2, LED3};
const enum btn_id_t btns[] = {BTN0, BTN1, BTN2, BTN3};

const size_t num_leds = sizeof(leds) / sizeof(leds[0]);
const size_t num_btns = sizeof(btns) / sizeof(btns[0]);

static void leds_off_state_entry(void* o);
static enum smf_state_result leds_off_state_run(void* o);

static void led1_blink_state_entry(void* o);
static enum smf_state_result led1_blink_state_run(void* o);
static void led1_blink_state_exit(void* o);

static void odd_leds_on_state_entry(void* o);
static enum smf_state_result odd_leds_on_state_run(void* o);
static void odd_leds_on_state_exit(void* o);

static void even_leds_on_state_entry(void* o);
static enum smf_state_result even_leds_on_state_run(void* o);
static void even_leds_on_state_exit(void* o);

static void leds_blink_state_entry(void* o);
static enum smf_state_result leds_blink_state_run(void* o);
static void leds_blink_state_exit(void* o);


enum led_state_machine_states 
{
    LEDS_OFF_STATE,
    LED1_BLINK_STATE,
    ODD_LEDS_ON_STATE,
    EVEN_LEDS_ON_STATE,
    LEDS_BLINK_STATE
};

typedef struct
{
    struct smf_ctx ctx;
    uint16_t count;
} led_state_object_t;

static const struct smf_state led_states[] = {
    [LEDS_OFF_STATE] = SMF_CREATE_STATE(leds_off_state_entry, leds_off_state_run, NULL, NULL, NULL),
    [LED1_BLINK_STATE] = SMF_CREATE_STATE(led1_blink_state_entry, led1_blink_state_run, led1_blink_state_exit, NULL, NULL),
    [ODD_LEDS_ON_STATE] = SMF_CREATE_STATE(odd_leds_on_state_entry, odd_leds_on_state_run, odd_leds_on_state_exit, NULL, NULL),
    [EVEN_LEDS_ON_STATE] = SMF_CREATE_STATE(even_leds_on_state_entry, even_leds_on_state_run, even_leds_on_state_exit, NULL, NULL),
    [LEDS_BLINK_STATE] = SMF_CREATE_STATE(leds_blink_state_entry, leds_blink_state_run, leds_blink_state_exit, NULL, NULL)
};

static led_state_object_t led_state_object;

void state_machine_init()
{
    led_state_object.count = 0;
    smf_set_initial(SMF_CTX(&led_state_object), &led_states[LEDS_OFF_STATE]);
}

int state_machine_run()
{
    return smf_run_state(SMF_CTX(&led_state_object));
}

int get_first_button_pressed()
{
    int ret = -1;
    for (int i = num_btns - 1; i >= 0; i--)
    {
        if (BTN_check_clear_pressed(btns[i]))
            ret = i;
    }
    return ret;
}

static void turn_all_leds_off()
{
    for (int i = 0; i < num_leds; i++)
    {
        LED_set(leds[i], LED_OFF);
    }
}

static void leds_off_state_entry(void* o)
{
    turn_all_leds_off();
}

static enum smf_state_result leds_off_state_run(void* o)
{
    if (get_first_button_pressed() == BTN0)
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LED1_BLINK_STATE]);
    return SMF_EVENT_HANDLED;
}

static void led1_blink_state_entry(void* o)  
{
    LED_blink(LED0, LED_4HZ);
}

static enum smf_state_result led1_blink_state_run(void* o)
{
    enum btn_id_t button_pressed = get_first_button_pressed();
    if (button_pressed == BTN1)
        smf_set_state(SMF_CTX(&led_state_object), &led_states[ODD_LEDS_ON_STATE]);
    if (button_pressed == BTN2)
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LEDS_BLINK_STATE]);
    if (button_pressed == BTN3)
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LEDS_OFF_STATE]);
    return SMF_EVENT_HANDLED;
}

static void led1_blink_state_exit(void* o)
{
    LED_set(LED0, LED_OFF);
}

static void odd_leds_on_state_entry(void* o)
{
    LED_set(LED0, LED_ON);
    LED_set(LED2, LED_ON);
}

static enum smf_state_result odd_leds_on_state_run(void* o)
{
    if (led_state_object.count < 1000)
        led_state_object.count++;
    else
    {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[EVEN_LEDS_ON_STATE]);
    }
    if (get_first_button_pressed() == BTN3)
    {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LEDS_OFF_STATE]);
    }
    return SMF_EVENT_HANDLED;
}

static void odd_leds_on_state_exit(void* o)
{
    LED_set(LED0, LED_OFF);
    LED_set(LED2, LED_OFF);
}

static void even_leds_on_state_entry(void* o)
{
    LED_set(LED1, LED_ON);
    LED_set(LED3, LED_ON);
}

static enum smf_state_result even_leds_on_state_run(void* o)
{
    if (led_state_object.count < 2000)
        led_state_object.count++;
    else
    {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[ODD_LEDS_ON_STATE]);
    }
    if (get_first_button_pressed() == BTN3)
    {
        led_state_object.count = 0;
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LEDS_OFF_STATE]);
    }
    return SMF_EVENT_HANDLED;
}

static void even_leds_on_state_exit(void* o)
{
    LED_set(LED1, LED_OFF);
    LED_set(LED3, LED_OFF);
}

static void leds_blink_state_entry(void* o)  
{
    for (int i = 0; i < num_leds; i++)
    {
        LED_blink(leds[i], LED_16HZ);
    }
}

static enum smf_state_result leds_blink_state_run(void* o)
{
    if (get_first_button_pressed() == BTN3)
        smf_set_state(SMF_CTX(&led_state_object), &led_states[LEDS_OFF_STATE]);
    return SMF_EVENT_HANDLED;
}

static void leds_blink_state_exit(void* o)
{
    turn_all_leds_off();
}
