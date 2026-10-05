#ifndef __BSP_SWITCH_H
#define __BSP_SWITCH_H

/**
 * @file bsp_switch.h
 * @brief 2 位拨码开关 (SW1~SW2) 硬件输入驱动与 60ms 软件消抖
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief 拨码开关通道枚举
     */
    typedef enum
    {
        SWITCH_ID_1 = 0,    /**< 拨码开关 1 */
        SWITCH_ID_2,        /**< 拨码开关 2 */
        SWITCH_ID_COUNT     /**< 拨码开关总数 (2) */
    } switch_id_t;

    /**
     * @brief 初始化拨码开关硬件与初始电平采样
     */
    void bsp_switch_init(void);

    /**
     * @brief 获取指定通道拨码开关的状态
     * @param[in] sw_id 开关通道枚举 (SWITCH_ID_1 或 SWITCH_ID_2)
     * @return true 处于闭合/高电平状态; false 处于断开/低电平状态
     */
    bool bsp_switch_get_state(switch_id_t sw_id);

    /**
     * @brief 获取经过 60ms 消抖后的拨码开关状态掩码
     * @return 掩码: bit0 对应 SW1, bit1 对应 SW2
     */
    uint8_t bsp_switch_get_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SWITCH_H */
