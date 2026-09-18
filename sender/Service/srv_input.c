#include "srv_input.h"
#include "system_config.h"
#include <string.h>
typedef struct
{
    bool candidate, stable, long_sent, wait_double, second_click;
    uint32_t edge, press, release;
} key_state_t;
static key_state_t keys[4];
static key_msg_t queue[32];
static uint8_t head, tail;
static uint32_t overflow;
static void event(unsigned id, key_event_t e)
{
    uint8_t next = (uint8_t)((head + 1U) % 32);
    if (next == tail)
    {
        overflow++;
        return;
    }
    queue[head].id = (key_id_t)id;
    queue[head].event = e;
    head = next;
}
void srv_keys_init(void)
{
    memset(keys, 0, sizeof(keys));
    head = tail = 0;
    overflow = 0;
}
void srv_keys_feed(uint8_t mask, uint32_t now)
{
    for (unsigned i = 0; i < 4; i++)
    {
        key_state_t *k = &keys[i];
        bool down = (mask & (1U << i)) != 0;
        if (down != k->candidate)
        {
            k->candidate = down;
            k->edge = now;
        }
        if (k->candidate != k->stable && (uint32_t)(now - k->edge) >= KEY_DEBOUNCE_MS)
        {
            k->stable = k->candidate;
            if (k->stable)
            {
                event(i, KEY_EVENT_PRESS);
                k->press = now;
                k->long_sent = false;
                k->second_click = k->wait_double && (uint32_t)(now - k->release) <= KEY_DOUBLE_MS;
                if (k->second_click)
                {
                    event(i, KEY_EVENT_DOUBLE_CLICK);
                    k->wait_double = false;
                }
                else
                    k->wait_double = false;
            }
            else
            {
                event(i, KEY_EVENT_RELEASE);
                k->wait_double = !k->long_sent && !k->second_click;
                k->release = now;
            }
        }
        if (k->stable && !k->long_sent && (uint32_t)(now - k->press) >= KEY_LONG_MS)
        {
            event(i, KEY_EVENT_LONG_PRESS);
            k->long_sent = true;
            k->wait_double = false;
        }
        if (k->wait_double && (uint32_t)(now - k->release) > KEY_DOUBLE_MS)
            k->wait_double = false;
    }
}
bool srv_keys_pop(key_msg_t *out)
{
    if (!out || head == tail)
        return false;
    *out = queue[tail];
    tail = (uint8_t)((tail + 1U) % 32);
    return true;
}
uint8_t srv_keys_mask(void)
{
    uint8_t mask = 0;
    for (unsigned i = 0; i < 4; i++)
        if (keys[i].stable)
            mask |= (uint8_t)(1U << i);
    return mask;
}
uint32_t srv_keys_overflows(void)
{
    return overflow;
}
int16_t srv_joystick_map(uint16_t mv, uint16_t center)
{
    if (mv > JOYSTICK_VREF_MV)
        mv = JOYSTICK_VREF_MV;
    if (center <= JOYSTICK_DEAD_MV || center >= JOYSTICK_VREF_MV - JOYSTICK_DEAD_MV)
        center = JOYSTICK_VREF_MV / 2;
    int32_t low = (int32_t)center - JOYSTICK_DEAD_MV, high = (int32_t)center + JOYSTICK_DEAD_MV;
    if (mv < low)
        return (int16_t)(((int32_t)mv - low) * 1000 / low);
    if (mv > high)
        return (int16_t)(((int32_t)mv - high) * 1000 / ((int32_t)JOYSTICK_VREF_MV - high));
    return 0;
}
