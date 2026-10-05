#pragma once

/**
 * @file sender_application.hpp
 * @brief 发送端 C++17 应用单例类定义
 */

extern "C"
{
#include "app_sender.h"
#include "srv_link.h"
}

namespace invc::sender
{
// 每个固件仅保留一个应用实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
/**
 * @brief 发送端应用管理器核心类 (单例、平凡构造/析构、零动态内存分配)
 */
class SenderApplication final
{
  public:
    SenderApplication() = default;
    SenderApplication(const SenderApplication &) = delete;
    SenderApplication &operator=(const SenderApplication &) = delete;

    /**
     * @brief 初始化发送端系统全部硬件、驱动、服务与状态机
     */
    void init();

    /**
     * @brief 发送端主循环任务 (驱动按键轮询、传感器采样、通信重传、菜单刷新与调试输出)
     */
    void task();

    /**
     * @brief 获取当前系统状态
     */
    app_state_t current_state();

    /**
     * @brief 获取当前遥测与状态视图只读快照
     */
    const sender_view_t *snapshot();

    /**
     * @brief 触发重新标定陀螺仪零偏
     */
    void recalibrate();

    /**
     * @brief 重置 UI 显示的发送/重传/确认基准计数
     */
    void reset_tx_count();

    /**
     * @brief 重置按键事件计数
     */
    void reset_events();

  private:
    /**
     * @brief 记录按键事件至异步调试日志队列
     * @param[in] key 按键消息
     * @param[in] now 时间戳
     */
    void log_key(key_msg_t key, uint32_t now);

    /**
     * @brief 异步将积压的按键日志通过串口打印输出
     */
    void drain_key_log();

    /**
     * @brief 无线串口物理发送适配函数 (适配 srv_link 回调)
     */
    static bool radio_send(const uint8_t *p, uint16_t n);

    /**
     * @brief 保持相位的无漂移软件时间片判断函数
     * @param[in]     now    当前时间戳
     * @param[in,out] last   上一触发周期基准时间戳
     * @param[in]     period 触发周期 (毫秒)
     * @return true 周期已到达; false 未到达
     */
    static bool due(uint32_t now, uint32_t *last, uint32_t period);

    /**
     * @brief 将浮点角度缩放并四舍五入为 0.1 度有符号整数
     */
    static int16_t angle10(float a);

    /**
     * @brief 采集摇杆与姿态数据并提交无线发送
     * @param[in] now 当前时间戳
     */
    void sample_and_send(uint32_t now);

    sender_view_t view;            /**< 聚合快照 */
    link_tx_t link;                /**< ARQ 发送链路状态机 */
    protocol_parser_t ack_parser;  /**< ACK 响应流式解析器 */
    uint32_t key_tick, sample_tick, slice_tick, debug_tick, filter_tick, rx_errors;
    uint32_t boot_tick;            /**< 开机时间戳 (用于开机静默) */
    uint32_t tx_base, ack_base, retry_base, expired_base; /**< 统计基准偏移 */
    bool imu_was_ready;            /**< 记录上一周期 IMU 就绪状态 (用于热插拔重标定) */
    uint8_t slice_interval;        /**< 屏幕分片刷新交替间隔 (12ms / 13ms) */

    /**
     * @brief 按键异步日志队列项
     */
    struct EventLogEntry
    {
        key_msg_t key;
        uint32_t tick;
    };
    EventLogEntry event_log[32];   /**< 按键日志环形缓冲 */
    uint8_t event_head, event_tail;/**< 日志环形指针 */
    uint32_t event_drops;          /**< 日志丢弃统计 */
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
/**
 * @brief 获取发送端单例对象引用
 */
SenderApplication &sender_application() noexcept;
} // invc::sender 命名空间
