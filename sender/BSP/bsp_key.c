#include "bsp_key.h"
#include <stddef.h>

void bsp_key_init(void)
{
    /* 按键 GPIO 状态机初始化 (PB0, PB1, PB10, PB11 内部上拉输入) */
}

void bsp_key_tick_10ms(void)
{
    /* 10ms 非阻塞消抖与状态转移逻辑骨架 */
}

bool bsp_key_get_event(key_msg_t *p_msg)
{
    if (p_msg == NULL) {
        return false;
    }
    p_msg->id = KEY_ID_1;
    p_msg->event = KEY_EVENT_NONE;
    return false;
}

uint8_t bsp_key_get_mask(void)
{
    return 0;
}
