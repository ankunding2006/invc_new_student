#include "bsp_switch.h"

void bsp_switch_init(void)
{
    /* 拨码开关 GPIO 初始化 (PB12, PB13 内部上拉输入) */
}

bool bsp_switch_get_state(switch_id_t sw_id)
{
    (void)sw_id;
    return false;
}

uint8_t bsp_switch_get_mask(void)
{
    return 0;
}
