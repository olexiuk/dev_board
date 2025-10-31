#include <zephyr/smf.h>

#include "BTN.h"
#include "LED.h"
#include "message_state_machine.h"

const enum led_id_t leds[] = {LED0, LED1, LED2, LED3};
const enum btn_id_t btns[] = {BTN0, BTN1, BTN2, BTN3};

const size_t num_leds = sizeof(leds) / sizeof(leds[0]);
const size_t num_btns = sizeof(btns) / sizeof(btns[0]);

static void character_entry_state_entry(void* o);
static enum smf_state_result character_entry_state_run(void* o);

enum message_state_machine_states 
{
    CHARACTER_ENTRY_STATE
};

typedef struct
{
    struct smf_ctx ctx;
    uint16_t count;
} message_state_object_t;

static const struct smf_state password_states[] =
{
    [CHARACTER_ENTRY_STATE] = SMF_CREATE_STATE(character_entry_state_entry, character_entry_state_run, NULL, NULL, NULL),
};

static message_state_object_t message_state_object;

void state_machine_init()
{
    message_state_object.count = 0;
    smf_set_initial(SMF_CTX(&message_state_object), &password_states[CHARACTER_ENTRY_STATE]);
}

int state_machine_run()
{
    return smf_run_state(SMF_CTX(&message_state_object));
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

static void character_entry_state_entry(void* o)
{
    LED_blink(LED3, LED_1HZ);
}

uint8_t character = 0;

static enum smf_state_result character_entry_state_run(void* o)
{
    int button_pressed = get_first_button_pressed();
    if (button_pressed == -1)
        return SMF_EVENT_HANDLED;
    else if (button_pressed == BTN0 || button_pressed == BTN1)
    {
        character = (character << 1 | button_pressed);
    }
    else if (button_pressed == BTN3)
    {
        printk("%c", character);
        character = 0;
    }
    else
    {
        character = 0;
    }
    return SMF_EVENT_HANDLED;
}
