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
static void character_entry_state_exit(void* o);

enum message_state_machine_states 
{
    CHARACTER_ENTRY_STATE,
    STRING_DECISION_STATE,
};

typedef struct
{
    struct smf_ctx ctx;
    uint16_t count;
} message_state_object_t;

static const struct smf_state message_states[] =
{
    [CHARACTER_ENTRY_STATE] = SMF_CREATE_STATE(character_entry_state_entry, character_entry_state_run, character_entry_state_exit, NULL, NULL),
    [STRING_DECISION_STATE] = SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
};

static message_state_object_t message_state_object;

void state_machine_init()
{
    message_state_object.count = 0;
    smf_set_initial(SMF_CTX(&message_state_object), &message_states[CHARACTER_ENTRY_STATE]);
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

char *str = NULL;
size_t index = 0;
size_t str_capacity = 0;

static int add_char_to_str(char character)
{
    if ((index + 1) * sizeof(char) > str_capacity)
    {
        str_capacity = str_capacity == 0 ? 16 : str_capacity * 2;
        char *tmp = k_realloc(str, str_capacity);
        if (tmp)
            str = tmp
        else
            return -1;
    }
    str[index] = character;
    str[++index] = "\0"
    return 0;
}

static void clear_str()
{
    index = 0;
    str_capacity = 0;
    str = NULL;
}

static void character_entry_state_entry(void* o)
{
    LED_blink(LED3, LED_1HZ);
}

uint8_t character = 0;

static enum smf_state_result character_entry_state_run(void* o)
{
    if (BTN_is_pressed(BTN0))
        LED_set(LED0, LED_ON);
    else
        LED_set(LED0, LED_OFF);

    if (BTN_is_pressed(BTN1))
        LED_set(LED1, LED_ON);
    else
        LED_set(LED1, LED_OFF);

    int button_pressed = get_first_button_pressed();
    if (button_pressed == -1)
        return SMF_EVENT_HANDLED;
    else if (button_pressed == BTN0 || button_pressed == BTN1)
        character = (character << 1 | button_pressed);
    else if (button_pressed == BTN2)
        character = 0;
    else if (button_pressed == BTN3)
    {
        ret = add_char_to_str(character)
        if (ret == -1)
            return SMFEVENT
        smf_set_state(SMF_CTX(&message_state_object), &message_states[STRING_DECISION_STATE]);
        character = 0;
    }

    return SMF_EVENT_HANDLED;
}

static void character_entry_state_exit(void* o)
{
    LED_set(LED3, LED_OFF);
}

static void string_decision_state_entry(void* o)
{
    LED_blink(LED3, LED_4HZ);
}

static enum smf_state_result string_decision_state_run(void* o)
{
    if (BTN_check_pressed(BTN0) == true || BTN_check_pressed(BTN1) == true)
        character = (character << 1 | get_first_button_pressed());
        smf_set_state(SMF_CTX(&message_state_object), &message_states[CHARACTER_ENTRY_STATE]);
    }
    else if ()
    {

    }
}

static void string_decision_state_exit(void* o)
{
    LED_set(LED3, LED_OFF);
}
