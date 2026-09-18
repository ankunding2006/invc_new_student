#ifndef BSP_KEY_H
#define BSP_KEY_H
#include "srv_input.h"
void bsp_key_init(void);
void bsp_key_tick_10ms(void);
bool bsp_key_get_event(key_msg_t *out);
uint8_t bsp_key_get_mask(void);
#endif
