#ifndef SRV_INPUT_H
#define SRV_INPUT_H

/**
 * @file srv_input.h
 * @brief 用户输入服务层 (按键消抖/多击/长按状态机与摇杆分段线性映射)
 */

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 按键编号枚举 (0~3 对应 K1~K4)
 */
typedef enum
{
    KEY_ID_1,      /**< 按键 1 (菜单上翻 / 增量) */
    KEY_ID_2,      /**< 按键 2 (菜单下翻 / 减量) */
    KEY_ID_3,      /**< 按键 3 (确认 / 进入详情 / 长按校准) */
    KEY_ID_4,      /**< 按键 4 (返回上一级) */
    KEY_ID_COUNT   /**< 按键总数 (4) */
} key_id_t;

/**
 * @brief 按键触发事件类型枚举
 */
typedef enum
{
    KEY_EVENT_NONE,        /**< 无事件 */
    KEY_EVENT_PRESS,       /**< 按下事件 (消抖后确认按下) */
    KEY_EVENT_RELEASE,     /**< 释放事件 (消抖后确认抬起) */
    KEY_EVENT_LONG_PRESS,  /**< 长按事件 (按下持续超过 2000ms) */
    KEY_EVENT_DOUBLE_CLICK /**< 双击事件 (释放后 300ms 内再次按下) */
} key_event_t;

/**
 * @brief 按键消息事件结构体
 */
typedef struct
{
    key_id_t id;       /**< 产生事件的按键编号 */
    key_event_t event; /**< 事件类型 */
} key_msg_t;

/**
 * @brief 初始化按键处理状态机与事件环形队列
 */
void srv_keys_init(void);

/**
 * @brief 周期性喂入硬件按键引脚电平掩码并驱动状态机演进
 * @param[in] pressed_mask 按键按下掩码 (bit0~bit3 分别代表 K1~K4 是否处于闭合按下状态)
 * @param[in] now          当前系统时间戳 (毫秒)
 * @note 建议以 10ms 左右周期调用。
 */
void srv_keys_feed(uint8_t pressed_mask, uint32_t now);

/**
 * @brief 从按键事件队列中弹出一个待处理的按键事件
 * @param[out] out 接收事件消息的结构体指针
 * @return true 成功获取事件; false 事件队列为空
 */
bool srv_keys_pop(key_msg_t *out);

/**
 * @brief 获取当前经过消抖确认后的稳定按键电平状态掩码
 * @return 4位按键掩码 (bit i 为 1 表示当前处于按下状态)
 */
uint8_t srv_keys_mask(void);

/**
 * @brief 获取按键事件队列溢出丢弃的次数统计
 * @return 溢出计数
 */
uint32_t srv_keys_overflows(void);

/**
 * @brief 摇杆物理采样电压归一化分段线性映射
 * 
 * 映射算法:
 *  - 输入范围: 0 ~ JOYSTICK_VREF_MV (3300mV)
 *  - 中心死区: [center_mv - JOYSTICK_DEAD_MV, center_mv + JOYSTICK_DEAD_MV] 映射为 0
 *  - 低于死区: 线性映射至 [-1000, 0)
 *  - 高于死区: 线性映射至 (0, +1000]
 * 
 * @param[in] mv        当前 ADC 实测采样电压 (毫伏 mV)
 * @param[in] center_mv 校准记录的摇杆静止中位参考电压 (毫伏 mV)
 * @return 归一化输出值 (-1000 ~ +1000)
 */
int16_t srv_joystick_map(uint16_t mv, uint16_t center_mv);

#endif
