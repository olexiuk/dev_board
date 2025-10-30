#include <zephyr/smf.h>

#include "BTN.h"
#include "LED.h"
#include "my_state_machine.h"

const enum led_id_t leds[] = {LED0, LED1, LED2, LED3};
const enum btn_id_t btns[] = {BTN0, BTN1, BTN2, BTN3};

const size_t num_leds = sizeof(leds) / sizeof(leds[0]);
const size_t num_btns = sizeof(btns) / sizeof(btns[0]);

static void setup_state_entry(void* o);
static enum smf_state_result setup_state_run(void* o);

static void setting_password_state_entry(void* o);
static enum smf_state_result setting_password_state_run(void* o);

static void locked_state_entry(void* o);
static enum smf_state_result locked_state_run(void* o);

static void unlocked_state_entry(void* o);
static enum smf_state_result unlocked_state_run(void* o);

enum password_state_machine_states 
{
    SETUP_STATE,
    SETTING_PASSWORD_STATE,
    LOCKED_STATE,
    UNLOCKED_STATE
};

typedef struct
{
    struct smf_ctx ctx;
    uint16_t count;
} password_state_object_t;

static const struct smf_state password_states[] = {
    [SETUP_STATE] = SMF_CREATE_STATE(setup_state_entry, setup_state_run, NULL, NULL, NULL),
    [SETTING_PASSWORD_STATE] = SMF_CREATE_STATE(setting_password_state_entry, setting_password_state_run, NULL, NULL, NULL),
    [LOCKED_STATE] = SMF_CREATE_STATE(locked_state_entry, locked_state_run, NULL, NULL, NULL),
    [UNLOCKED_STATE] = SMF_CREATE_STATE(unlocked_state_entry, unlocked_state_run, NULL, NULL, NULL),
};

static password_state_object_t password_state_object;

void state_machine_init()
{
    password_state_object.count = 0;
    smf_set_initial(SMF_CTX(&password_state_object), &password_states[SETUP_STATE]);
}

int state_machine_run()
{
    return smf_run_state(SMF_CTX(&password_state_object));
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


// each two bits represents a button
// password can be different lengths

// password is 1233
// uint16_t password = 0b01101111;
// uint8_t passwordLength = 4;

// password is 12332133
uint16_t password = 0b0110111110011111;
uint8_t password_length = 8;
uint16_t input = 0;

// used to clear any bits past the length of the password
uint16_t password_cap;


static void setup_state_entry(void* o)
{
    LED_set(LED3, LED_ON);
}

static enum smf_state_result setup_state_run(void* o)
{
    if (password_state_object.count < 3000)
        password_state_object.count++;
    else
    {
        password_state_object.count = 0;
        password_cap = 0b1111111111111111 >> (16 - (password_length * 2));
        smf_set_state(SMF_CTX(&password_state_object), &password_states[LOCKED_STATE]);
    }
    if (get_first_button_pressed() == BTN3)
    {
        password_state_object.count = 0;
        smf_set_state(SMF_CTX(&password_state_object), &password_states[SETTING_PASSWORD_STATE]);
    }
    return SMF_EVENT_HANDLED;
}

static void setting_password_state_entry(void* o)
{
    password = 0;
    password_length = 0;
}

static enum smf_state_result setting_password_state_run(void* o)
{
    int button_pressed = get_first_button_pressed(btns);
    if (button_pressed == -1)
        return SMF_EVENT_HANDLED;
    if (button_pressed == BTN3)
    {
        password_cap = 0b1111111111111111 >> (16 - (password_length * 2));
        smf_set_state(SMF_CTX(&password_state_object), &password_states[LOCKED_STATE]);
    }
    else
    {
        password = (password << 2 | (button_pressed + 1));
        if (password_length < 8)
            password_length++;
    }
    return SMF_EVENT_HANDLED;
}

static void locked_state_entry(void* o)
{
    LED_set(LED3, LED_OFF);
    LED_set(LED0, LED_ON);
}

static enum smf_state_result locked_state_run(void* o)
{
    int button_pressed = get_first_button_pressed();
    if (button_pressed == -1)
        return SMF_EVENT_HANDLED;
    if (button_pressed == BTN3)
    {
        if (password == input)
        {
            printk("Correct!\n");
            smf_set_state(SMF_CTX(&password_state_object), &password_states[UNLOCKED_STATE]);
        }
        else
            printk("Incorrect!\n");
        input = 0;
    }
    else
        // add button pressed to front of input and clear bits that are passed the length of the password
        input = (input << 2 | (button_pressed + 1)) & password_cap;
    return SMF_EVENT_HANDLED;
}

static void unlocked_state_entry(void* o)
{
    LED_set(LED0, LED_OFF);
}

static enum smf_state_result unlocked_state_run(void* o)
{
    if (get_first_button_pressed() != -1)
        smf_set_state(SMF_CTX(&password_state_object), &password_states[LOCKED_STATE]);
    return SMF_EVENT_HANDLED;
}
