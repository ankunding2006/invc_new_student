#ifndef SRV_LINK_H
#define SRV_LINK_H

/**
 * @file srv_link.h
 * @brief 发送端无线链路层 ARQ (停等重传/确认协议) 服务
 * 
 * 核心机制:
 * 1. 停等 ARQ (Stop-and-Wait ARQ) 与超时重传 (LINK_ACK_TIMEOUT_MS)；
 * 2. 具有最大重传次数限制 (LINK_MAX_RETRIES)，超时未收到 ACK 则丢弃当前帧并切换序列号；
 * 3. 绝对超时强制丢弃保护，防止通信阻塞造成遥测数据时延累积；
 * 4. 采用 16 位 CRC 令牌与 seq 共同验证 ACK 的有效性。
 */

#include "srv_protocol.h"

/**
 * @brief 无线底层发送函数指针类型
 * @param[in] data 待发送字节缓冲区指针
 * @param[in] len  发送字节数
 * @return true 启动发送成功; false 硬件繁忙或失败
 */
typedef bool (*link_send_fn)(const uint8_t *, uint16_t);

/**
 * @brief 发送端无线链路层状态机管理结构体
 */
typedef struct
{
    uint8_t frame[23], seq, retries; /* 待重传缓存帧 (23字节)、当前包序号 (0~255)、已重传次数 */
    uint16_t token;                   /* 当前帧对应的 CRC-16 令牌，用于匹配确认 ACK */
    uint32_t sent_ms, first_ms, total_tx, total_retry, total_acked, total_expired, last_ack_ms;
    /* sent_ms: 最近一次发送时刻; first_ms: 首次发送时刻; total_tx: 累计发送总次数;
     * total_retry: 累计重传总次数; total_acked: 累计确认成功总数; total_expired: 累计超时丢弃帧数;
     * last_ack_ms: 最近一次收到合法 ACK 的时刻 */
    bool pending, ever_acked;        /* pending: 存在正在等待 ACK 的待确认帧; ever_acked: 历史上是否曾成功收到过 ACK */
    link_send_fn send;                /* 底层串口发送回调函数接口 */
} link_tx_t;

/**
 * @brief 初始化发送端链路 ARQ 状态机
 * @param[out] s    链路状态结构体指针
 * @param[in]  send 底层硬件发送函数指针
 */
void srv_link_init(link_tx_t *s, link_send_fn send);

/**
 * @brief 提交一帧新的遥测数据并立即启动首次无线发送
 * @param[in,out] s    链路状态结构体指针
 * @param[in]     data 遥测核心载荷数据指针
 * @param[in]     now  当前系统时间戳 (毫秒)
 * @return true 提交并发送成功; false 尚有前一帧在等待确认(pending)或底层忙
 */
bool srv_link_submit(link_tx_t *s, const telemetry_payload_t *data, uint32_t now);

/**
 * @brief 周期性轮询链路超时与重传状态机
 * @param[in,out] s   链路状态结构体指针
 * @param[in]     now 当前系统时间戳 (毫秒)
 * @note 必须在主循环任务中定期调用，用于判定重传超时与丢包放弃。
 */
void srv_link_poll(link_tx_t *s, uint32_t now);

/**
 * @brief 接收并处理对端发回的 ACK 应答包
 * @param[in,out] s   链路状态结构体指针
 * @param[in]     ack 接收到的协议包指针
 * @param[in]     now 当前系统时间戳 (毫秒)
 * @return true 成功匹配当前等待的 ACK 并解除 pending 状态; false 序号不匹配、令牌无效或非 ACK
 */
bool srv_link_ack(link_tx_t *s, const protocol_packet_t *ack, uint32_t now);

#endif
