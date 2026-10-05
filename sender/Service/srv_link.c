/**
 * @file srv_link.c
 * @brief 发送端无线链路层停等重传 (ARQ) 状态机实现
 */

#include "srv_link.h"
#include "system_config.h"
#include <string.h>

/**
 * @brief 初始化链路层发送管理器
 * @param[out] s    链路状态结构体指针
 * @param[in]  send 绑定的无线物理层发送函数
 */
void srv_link_init(link_tx_t *s, link_send_fn send)
{
    if (s)
    {
        memset(s, 0, sizeof(*s));
        s->send = send;
    }
}

/**
 * @brief 提交新的遥测帧并启动发送
 * @param[in,out] s   链路状态结构体指针
 * @param[in]     p   遥测载荷数据指针
 * @param[in]     now 当前毫秒时间戳
 * @return true 提交并发射成功; false 尚有未确认帧处于 pending 状态或发送失败
 */
bool srv_link_submit(link_tx_t *s, const telemetry_payload_t *p, uint32_t now)
{
    /* 若已有帧正在等待 ACK (pending) 或未注册发送函数，拒绝提交新帧 */
    if (!s || !p || s->pending || !s->send)
        return false;
    /* 打包 23 字节遥测数据帧 */
    srv_protocol_pack(s->seq, p, s->frame, sizeof(s->frame));
    /* 调用底层无线串口非阻塞发送 */
    if (!s->send(s->frame, 23))
        return false;
    /* 计算并记录该帧的 CRC-16 令牌，用于后续精确匹配 ACK */
    s->token = srv_protocol_token(s->frame, 23);
    s->pending = true;
    s->retries = 0;
    s->sent_ms = s->first_ms = now;
    s->total_tx++;
    return true;
}

/**
 * @brief 周期轮询处理超时重传与丢包放弃
 * @param[in,out] s   链路状态结构体指针
 * @param[in]     now 当前毫秒时间戳
 */
void srv_link_poll(link_tx_t *s, uint32_t now)
{
    /* 若无正在等待确认的帧，无需执行操作 */
    if (!s || !s->pending)
        return;
    /* 1. 总寿命超时检查: 从首次发送至今超过整个重传周期的总允许时间，强制放弃此帧 */
    if ((uint32_t)(now - s->first_ms) >= LINK_ACK_TIMEOUT_MS * (LINK_MAX_RETRIES + 1U))
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    /* 2. 单次重传间隔检查: 距上次发送未达到 80ms 超时时间，继续等待 ACK */
    if ((uint32_t)(now - s->sent_ms) < LINK_ACK_TIMEOUT_MS)
        return;
    /* 3. 重传次数上限检查: 已达最大重传次数 (LINK_MAX_RETRIES=2)，放弃重传 */
    if (s->retries >= LINK_MAX_RETRIES)
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    /* 4. 绝对超时保护: 防止串口驱动或定时器回绕导致的重传悬挂 */
    /* Absolute expiry prevents a busy UART from retaining an old sample forever. */
    if ((uint32_t)(now - s->sent_ms) >= LINK_ACK_TIMEOUT_MS * 2U)
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    /* 执行重传并递增统计计数 */
    if (s->send && s->send(s->frame, 23))
    {
        s->sent_ms = now;
        s->retries++;
        s->total_tx++;
        s->total_retry++;
    }
}

/**
 * @brief 处理接收到的 ACK 应答帧
 * @param[in,out] s   链路状态结构体指针
 * @param[in]     p   协议解包后的数据包结构体指针
 * @param[in]     now 当前毫秒时间戳
 * @return true 确认为当前正在等待的有效 ACK 并成功解除 pending; false 无效或不匹配
 */
bool srv_link_ack(link_tx_t *s, const protocol_packet_t *p, uint32_t now)
{
    /* 校验功能码是否为 CMD_ACK、载荷是否为 2 字节令牌、序列号 seq 是否与当前等待帧一致 */
    if (!s || !p || !s->pending || p->cmd != PROTOCOL_CMD_ACK || p->len != 2 || p->seq != s->seq)
        return false;
    /* 提取对端返回的 16 位 CRC 令牌 (小端序) */
    uint16_t token = (uint16_t)(p->payload[0] | ((uint16_t)p->payload[1] << 8));
    /* 比对令牌是否与此前记录的帧令牌一致 */
    if (token != s->token)
        return false;
    /* ACK 校验成功，解除等待状态，序号自增准备下一帧 */
    s->pending = false;
    s->seq++;
    s->total_acked++;
    s->last_ack_ms = now;
    s->ever_acked = true;
    return true;
}
