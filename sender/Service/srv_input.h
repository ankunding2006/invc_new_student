#ifndef SRV_INPUT_H
#define SRV_INPUT_H
#include <stdint.h>
#include <stdbool.h>
typedef enum
{
    KEY_ID_1,
    KEY_ID_2,
    KEY_ID_3,
    KEY_ID_4,
    KEY_ID_COUNT
} key_id_t;
typedef enum
{
    KEY_EVENT_NONE,
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_DOUBLE_CLICK
} key_event_t;
typedef struct
{
    key_id_t id;
    key_event_t event;
} key_msg_t;
void srv_keys_init(void);
void srv_keys_feed(uint8_t pressed_mask, uint32_t now);
bool srv_keys_pop(key_msg_t *out);
uint8_t srv_keys_mask(void);
uint32_t srv_keys_overflows(void);
int16_t srv_joystick_map(uint16_t mv, uint16_t center_mv);
#endif
