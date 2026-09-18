#include "bsp_key.h"
#include "main.h"
void bsp_key_init(void)
{
    srv_keys_init();
}
void bsp_key_tick_10ms(void)
{
    uint32_t pins = GPIOB->IDR;
    uint8_t mask = 0;
    const uint16_t bits[4] = {KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin};
    for (unsigned i = 0; i < 4; i++)
        if (!(pins & bits[i]))
            mask |= (uint8_t)(1U << i);
    srv_keys_feed(mask, HAL_GetTick());
}
bool bsp_key_get_event(key_msg_t *out)
{
    return srv_keys_pop(out);
}
uint8_t bsp_key_get_mask(void)
{
    return srv_keys_mask();
}
