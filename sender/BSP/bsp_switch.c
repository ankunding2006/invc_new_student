/**
 * @file bsp_switch.c
 * @brief 拨码开关电平读取与 60ms 软件消抖实现
 */

#include "bsp_switch.h"
#include "main.h"

/** @brief 稳定电平掩码与候选跳变电平掩码 */
static uint8_t stable, candidate;
/** @brief 各通道发生电平变动的时刻时间戳 */
static uint32_t changed[2];

/**
 * @brief 内部直接读取 GPIO 引脚当前瞬时电平并组合为 2 位掩码
 * @return 瞬时掩码 (bit0: SW1, bit1: SW2)
 */
static uint8_t pins(void)
{
    uint32_t value = GPIOB->IDR;
    return (uint8_t)(((value & SW1_Pin) ? 1U : 0U) | ((value & SW2_Pin) ? 2U : 0U));
}

/**
 * @brief 初始化拨码开关引脚状态
 */
void bsp_switch_init(void)
{
    stable = candidate = pins();
    changed[0] = changed[1] = HAL_GetTick();
}

/**
 * @brief 获取消抖后的拨码开关掩码
 * @return 掩码
 */
uint8_t bsp_switch_get_mask(void)
{
    uint8_t current = pins();
    uint32_t now = HAL_GetTick();
    /* 分别对 SW1 和 SW2 进行 60ms 稳定消抖滤波 */
    for (unsigned i = 0; i < 2; i++)
    {
        uint8_t bit = (uint8_t)(1U << i);
        if ((current ^ candidate) & bit)
        {
            /* 电平发生跳变，重置候选电平与计时点 */
            candidate ^= bit;
            changed[i] = now;
        }
        else if ((uint32_t)(now - changed[i]) >= 60U)
            /* 电平持续稳定保持超过 60ms，确认更新至稳定掩码中 */
            stable = (uint8_t)((stable & ~bit) | (candidate & bit));
    }
    return stable;
}

/**
 * @brief 查询单个拨码开关状态
 * @param[in] id 开关编号
 * @return true 处于开启状态; false 处于关闭状态
 */
bool bsp_switch_get_state(switch_id_t id)
{
    return (unsigned)id < SWITCH_ID_COUNT && (bsp_switch_get_mask() & (1U << id)) != 0;
}
