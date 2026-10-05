/**
 * @file srv_input.c
 * @brief 按键多状态机与摇杆分段线性死区映射实现
 */

#include "srv_input.h"
#include "system_config.h"
#include <string.h>

/**
 * @brief 单按键状态跟踪结构体
 */
typedef struct
{
    bool candidate, stable, long_sent, wait_double, second_click;
    /* candidate: 待确认电平; stable: 消抖确认电平; long_sent: 是否已发长按;
     * wait_double: 等待双击第二击; second_click: 确认为双击第二击 */
    uint32_t edge, press, release;
    /* edge: 电平跳变时刻; press: 按下时刻; release: 抬起释放时刻 */
} key_state_t;

/** @brief 4 个物理按键独立的状态机实例 */
static key_state_t keys[4];
/** @brief 环形按键事件队列 (容量 32) */
static key_msg_t queue[32];
/** @brief 事件队列头尾游标 */
static uint8_t head, tail;
/** @brief 队列溢出丢弃计数 */
static uint32_t overflow;

/**
 * @brief 内部生成按键事件并压入事件队列
 * @param[in] id 按键编号 (0~3)
 * @param[in] e  事件类型
 */
static void event(unsigned id, key_event_t e)
{
    uint8_t next = (uint8_t)((head + 1U) % 32);
    /* 队列已满则累加溢出丢弃计数 */
    if (next == tail)
    {
        overflow++;
        return;
    }
    queue[head].id = (key_id_t)id;
    queue[head].event = e;
    head = next;
}

/**
 * @brief 初始化按键服务模块
 */
void srv_keys_init(void)
{
    memset(keys, 0, sizeof(keys));
    head = tail = 0;
    overflow = 0;
}

/**
 * @brief 周期喂入按键输入掩码并更新状态机
 * @param[in] mask 4 位按键电平掩码 (bit i=1 代表闭合/按下)
 * @param[in] now  当前毫秒时间戳
 */
void srv_keys_feed(uint8_t mask, uint32_t now)
{
    for (unsigned i = 0; i < 4; i++)
    {
        key_state_t *k = &keys[i];
        bool down = (mask & (1U << i)) != 0;
        /* 1. 检测边沿跳变: 若输入电平与待确认候选电平不一致，记录跳变时刻 */
        if (down != k->candidate)
        {
            k->candidate = down;
            k->edge = now;
        }
        /* 2. 软件消抖判定: 候选电平与稳定电平不同且维持时间超过消抖阈值 (KEY_DEBOUNCE_MS=20ms) */
        if (k->candidate != k->stable && (uint32_t)(now - k->edge) >= KEY_DEBOUNCE_MS)
        {
            k->stable = k->candidate;
            if (k->stable)
            {
                /* 产生按下事件 */
                event(i, KEY_EVENT_PRESS);
                k->press = now;
                k->long_sent = false;
                /* 判断是否落在双击时间窗口内 (距上次释放 <= KEY_DOUBLE_MS=300ms) */
                k->second_click = k->wait_double && (uint32_t)(now - k->release) <= KEY_DOUBLE_MS;
                if (k->second_click)
                {
                    event(i, KEY_EVENT_DOUBLE_CLICK);
                    k->wait_double = false;
                }
                else
                    k->wait_double = false;
            }
            else
            {
                /* 产生释放抬起事件 */
                event(i, KEY_EVENT_RELEASE);
                /* 若不是长按且不是双击的第二击，则开启等待双击计时 */
                k->wait_double = !k->long_sent && !k->second_click;
                k->release = now;
            }
        }
        /* 3. 长按判定: 按下状态持续达到 KEY_LONG_MS (2000ms)，触发长按事件 */
        if (k->stable && !k->long_sent && (uint32_t)(now - k->press) >= KEY_LONG_MS)
        {
            event(i, KEY_EVENT_LONG_PRESS);
            k->long_sent = true;
            k->wait_double = false;
        }
        /* 4. 双击超时清除: 释放后超过 300ms 未按下第二击，清除双击等待标志 */
        if (k->wait_double && (uint32_t)(now - k->release) > KEY_DOUBLE_MS)
            k->wait_double = false;
    }
}

/**
 * @brief 从队列中取出首个待处理按键事件
 * @param[out] out 接收事件消息指针
 * @return true 成功获取; false 队列为空
 */
bool srv_keys_pop(key_msg_t *out)
{
    if (!out || head == tail)
        return false;
    *out = queue[tail];
    tail = (uint8_t)((tail + 1U) % 32);
    return true;
}

/**
 * @brief 获取消抖后的按键稳定电平掩码
 * @return 掩码字节 (低 4 位为 K1~K4)
 */
uint8_t srv_keys_mask(void)
{
    uint8_t mask = 0;
    for (unsigned i = 0; i < 4; i++)
        if (keys[i].stable)
            mask |= (uint8_t)(1U << i);
    return mask;
}

/**
 * @brief 获取事件队列溢出次数
 * @return 溢出次数
 */
uint32_t srv_keys_overflows(void)
{
    return overflow;
}

/**
 * @brief 摇杆物理采样电压映射为归一化量 (-1000 ~ +1000)
 * @param[in] mv     采样电压 (mV)
 * @param[in] center 标定的中位电压 (mV)
 * @return 归一化值 (-1000 ~ +1000)
 */
int16_t srv_joystick_map(uint16_t mv, uint16_t center)
{
    /* 电压限幅至供电参考电压 3300mV */
    if (mv > JOYSTICK_VREF_MV)
        mv = JOYSTICK_VREF_MV;
    /* 异常中位参考保护: 若标定中点越界则使用理论半满量程 1650mV */
    if (center <= JOYSTICK_DEAD_MV || center >= JOYSTICK_VREF_MV - JOYSTICK_DEAD_MV)
        center = JOYSTICK_VREF_MV / 2;
    /* 计算中心死区上下界 */
    int32_t low = (int32_t)center - JOYSTICK_DEAD_MV, high = (int32_t)center + JOYSTICK_DEAD_MV;
    /* 低于死区下界: 线性映射至 [-1000, 0) */
    if (mv < low)
        return (int16_t)(((int32_t)mv - low) * 1000 / low);
    /* 高于死区上界: 线性映射至 (0, +1000] */
    if (mv > high)
        return (int16_t)(((int32_t)mv - high) * 1000 / ((int32_t)JOYSTICK_VREF_MV - high));
    /* 死区范围内输出 0 */
    return 0;
}
