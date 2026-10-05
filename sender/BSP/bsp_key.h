#ifndef BSP_KEY_H
#define BSP_KEY_H

/**
 * @file bsp_key.h
 * @brief 4 位板载机械按键 (K1~K4) 硬件引脚读取驱动
 */

#include "srv_input.h"

/**
 * @brief 初始化按键硬件模块并复位按键状态机
 */
void bsp_key_init(void);

/**
 * @brief 10ms 周期采样按键 GPIO 引脚并喂入输入服务状态机
 * @note 读取 GPIOB 的 IDR 寄存器，按键为低电平有效 (按下接地拉低)
 */
void bsp_key_tick_10ms(void);

/**
 * @brief 获取下一个按键消息事件 (按下/释放/长按/双击)
 * @param[out] out 接收事件消息结构体指针
 * @return true 成功获取事件; false 事件队列为空
 */
bool bsp_key_get_event(key_msg_t *out);

/**
 * @brief 获取当前经过消抖确认的 4 位按键电平状态掩码
 * @return bit0~bit3 对应 K1~K4 (1 代表按下)
 */
uint8_t bsp_key_get_mask(void);

#endif
