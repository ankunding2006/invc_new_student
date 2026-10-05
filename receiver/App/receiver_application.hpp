#pragma once

/**
 * @file receiver_application.hpp
 * @brief 接收端 C++17 应用单例类定义
 */

extern "C"
{
#include "app_receiver.h"
}

namespace invc::receiver
{
// 每个固件仅保留一个应用实例：底层 C 服务维护硬件和算法状态。
// 构造过程不会访问硬件；请调用 init() 完成初始化。
/**
 * @brief 接收端应用核心管理类 (单例、零动态内存分配)
 */
class ReceiverApplication final
{
  public:
    ReceiverApplication() = default;
    ReceiverApplication(const ReceiverApplication &) = delete;
    ReceiverApplication &operator=(const ReceiverApplication &) = delete;

    /**
     * @brief 初始化接收端驱动、协议解析器、统计模块与屏幕
     */
    void init();

    /**
     * @brief 接收端主调度任务 (处理无线数据接收、校验、去重、回复 ACK、转发 PC 上位机、刷新屏幕)
     */
    void task();

    /**
     * @brief 获取当前连接状态
     */
    receiver_state_t current_state();

    /**
     * @brief 获取遥测数据快照指针
     */
    const telemetry_payload_t *snapshot();

    /**
     * @brief 获取上位机转发丢包数
     */
    uint32_t forwarding_drops();

  private:
    /**
     * @brief 将当前遥测数据格式化后转发给 PC 上位机 (支持二进制浮点模式或 ASCII 文本模式)
     */
    bool forward();

    /**
     * @brief 严格校验遥测数据载荷各物理量的合法取值范围 (越界保护)
     */
    static bool valid_payload(const telemetry_payload_t *p);

    receiver_state_t state;        /**< 接收端链路连接状态 */
    protocol_parser_t parser;      /**< 无线接收流式协议解析器 */
    telemetry_payload_t telemetry; /**< 最新有效遥测数据缓存 */
    stats_metrics_t metrics;       /**< 通信统计指标 (频率、丢包率等) */
    uint32_t last_packet, stats_tick, slice_tick, rx_errors, pc_drops;
    /* last_packet: 最近收到包时刻; stats_tick: 统计结算时刻; slice_tick: 屏幕分片刷新时刻;
     * rx_errors: 串口接收错误基准; pc_drops: 上位机转发丢弃数 */
    uint8_t slice_interval, last_seq, ack[9];
    /* slice_interval: 分片刷新时间间隔; last_seq: 最近包序号; ack[9]: 待回复的 9 字节 ACK 缓冲区 */
    bool ack_pending, text_mode, stats_log_pending;
    /* ack_pending: 存在待发送 ACK; text_mode: 上位机协议模式 (true 为文本，false 为 60 字节浮点);
     * stats_log_pending: 统计结算日志待发送标志 */
};

// 供 C ABI 桥接层和 C++ 调用方共用；不进行动态内存分配。
/**
 * @brief 获取接收端应用单例对象引用
 */
ReceiverApplication &receiver_application() noexcept;
} // invc::receiver 命名空间
