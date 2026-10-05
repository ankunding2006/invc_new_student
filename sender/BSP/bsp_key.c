/**
 * @file bsp_key.c
 * @brief 按键 GPIO 引脚读取与状态机喂入实现
 */

#include "bsp_key.h"
#include "main.h"

/**
 * @brief 初始化按键服务状态机
 */
void bsp_key_init(void)
{
    srv_keys_init();
}

/**
 * @brief 周期性采样按键引脚电平并更新按键消抖状态机
 * @note 硬件接线为外部/内部上拉，按键按下时对应引脚接地呈现低电平 (Low-Active)
 */
void bsp_key_tick_10ms(void)
{
    /* 一次性原子读取 GPIOB 输入数据寄存器 (IDR) */
    uint32_t pins = GPIOB->IDR;
    uint8_t mask = 0;
    const uint16_t bits[4] = {KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin};
    /* 遍历 4 个按键引脚，低电平取反为逻辑 1 (有效按下) */
    for (unsigned i = 0; i < 4; i++)
        if (!(pins & bits[i]))
            mask |= (uint8_t)(1U << i);
    /* 将 4 位按键电平掩码与当前系统时间戳喂入输入服务层 */
    srv_keys_feed(mask, HAL_GetTick());
}

/**
 * @brief 从按键事件队列中弹出待处理事件
 * @param[out] out 接收事件消息指针
 * @return true 成功获取事件; false 队列为空
 */
bool bsp_key_get_event(key_msg_t *out)
{
    return srv_keys_pop(out);
}

/**
 * @brief 获取稳定的按键电平掩码
 * @return 掩码
 */
uint8_t bsp_key_get_mask(void)
{
    return srv_keys_mask();
}
