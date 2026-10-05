/**
 * @file receiver_application.cpp
 * @brief 接收端核心业务逻辑与流程调度实现
 */

#include "receiver_application.hpp"
extern "C"
{
#include "app_receiver.h"
#include "app_ui.h"
#include "srv_pc.h"
#include "system_config.h"
#include "bsp_oled.h"
#include "bsp_usart.h"
#include "main.h"
}
#include <string.h>
#include <stdio.h>
#include <type_traits>

namespace invc::receiver
{
namespace
{
// 零初始化的静态存储，保持原有 C 启动状态。
static_assert(std::is_trivially_default_constructible_v<ReceiverApplication>);
static_assert(std::is_trivially_destructible_v<ReceiverApplication>);
/** @brief 接收端单例实例 */
ReceiverApplication instance{};
} // 匿名命名空间

/**
 * @brief 获取接收端单例对象引用
 */
ReceiverApplication &receiver_application() noexcept
{
    return instance;
}

/**
 * @brief 获取当前遥测数据快照指针
 */
const telemetry_payload_t *ReceiverApplication::snapshot(void)
{
    return &telemetry;
}

/**
 * @brief 获取当前接收端连接状态
 */
receiver_state_t ReceiverApplication::current_state(void)
{
    return state;
}

/**
 * @brief 获取上位机转发丢包总数
 */
uint32_t ReceiverApplication::forwarding_drops(void)
{
    return pc_drops;
}

/**
 * @brief 将遥测与统计数据转发给上位机
 * @return true 转发成功; false 串口忙或丢弃
 */
bool ReceiverApplication::forward(void)
{
    uint8_t output[192];
    uint16_t len;
    /* 1. 文本协议模式 ('T'/'t' 触发) */
    if (text_mode)
    {
        int n = snprintf((char *)output, sizeof output,
                         "R id=%u x=%d y=%d mv=%u,%u key=%X sw=%X deg10=%d,%d,%d hz10=%u loss10=%u "
                         "flags=%X link=%u\r\n",
                         last_seq, telemetry.joy_x_raw, telemetry.joy_y_raw, telemetry.joy_x_mv,
                         telemetry.joy_y_mv, telemetry.key_mask & 15, telemetry.switch_mask & 3,
                         telemetry.pitch_cd, telemetry.roll_cd, telemetry.yaw_cd,
                         (unsigned)(metrics.freq_hz * 10), (unsigned)(metrics.loss_rate_pct * 10),
                         telemetry.key_mask >> 4, (unsigned)state);
        len = n > 0 ? (uint16_t)(n < (int)sizeof output ? n : (int)sizeof(output) - 1) : 0;
    }
    /* 2. VOFA+ FireWater 60 字节二进制浮点协议模式 ('F'/'f' 触发) */
    else
        len = srv_pc_pack(&telemetry, metrics.freq_hz, metrics.loss_rate_pct, metrics.wire_freq_hz,
                          state == RECEIVER_STATE_CONNECTED, output, sizeof output);
    /* 通过 USART2 向上位机发送 */
    if (len && !bsp_usart_transmit(USART_PORT_PC_FORWARD, output, len))
    {
        pc_drops++;
        return false;
    }
    return len != 0;
}

/**
 * @brief 初始化接收端所有外设、驱动与服务
 */
void ReceiverApplication::init(void)
{
    state = RECEIVER_STATE_WAIT_SYNC;
    memset(&telemetry, 0, sizeof telemetry);
    memset(&metrics, 0, sizeof metrics);
    bsp_usart_init();
    bsp_oled_init();
    srv_stats_init();
    protocol_parser_init(&parser);
    app_ui_init();
    last_packet = stats_tick = slice_tick = HAL_GetTick();
    slice_interval = 12;
    rx_errors = pc_drops = 0;
    ack_pending = text_mode = stats_log_pending = false;
    last_seq = 0;
}

/**
 * @brief 防御性安全边界校验: 确保遥测载荷各数值符合物理界限
 * @param[in] p 遥测数据指针
 * @return true 数据均在合理物理范围内; false 存在越界数值 (非法损坏包)
 */
bool ReceiverApplication::valid_payload(const telemetry_payload_t *p)
{
    return p->joy_x_raw >= -1000 && p->joy_x_raw <= 1000 && p->joy_y_raw >= -1000 &&
           p->joy_y_raw <= 1000 && p->joy_x_mv <= 3300 && p->joy_y_mv <= 3300 &&
           p->switch_mask <= 3 && p->pitch_cd >= -1800 && p->pitch_cd <= 1800 &&
           p->roll_cd >= -1800 && p->roll_cd <= 1800 && p->yaw_cd >= 0 && p->yaw_cd <= 3600;
}

/**
 * @brief 接收端主调度任务函数
 */
void ReceiverApplication::task(void)
{
    bool published = false;
    uint32_t now = HAL_GetTick();

    /* 1. 串口驱动后台维护 */
    bsp_usart_service(now);

    /* 2. 检查链路连接是否超时断开 (LINK_OFFLINE_MS=1000ms) */
    if (state == RECEIVER_STATE_CONNECTED && (uint32_t)(now - last_packet) > LINK_OFFLINE_MS)
    {
        state = RECEIVER_STATE_OFFLINE;
        srv_stats_resync();
        metrics.freq_hz = metrics.wire_freq_hz = 0;
        published = forward();
    }

    /* 3. 统计周期窗口结算 (每 1000ms 统计一次帧率和丢包率) */
    /* 统计新收到的数据包前，先结算上一统计窗口。 */
    if ((uint32_t)(now - stats_tick) >= 1000)
    {
        srv_stats_tick(now - stats_tick);
        stats_tick = now;
        srv_stats_get_metrics(&metrics);
        if (state != RECEIVER_STATE_CONNECTED)
            metrics.freq_hz = metrics.wire_freq_hz = 0;
        stats_log_pending = true;
    }

    /* 4. 处理上位机下发的模式切换指令 ('T' 切换文本模式，'F' 切换浮点模式) */
    uint8_t bytes[96];
    uint16_t n = bsp_usart_receive(USART_PORT_PC_FORWARD, bytes, sizeof bytes);
    for (unsigned i = 0; i < n; i++)
    {
        if (bytes[i] == 'T' || bytes[i] == 't')
            text_mode = true;
        else if (bytes[i] == 'F' || bytes[i] == 'f')
            text_mode = false;
    }

    /* 5. 检查无线串口接收错误并自愈 */
    uint32_t errors = bsp_usart_rx_errors(USART_PORT_WIRELESS);
    if (errors != rx_errors)
    {
        parser.used = 0;
        bsp_usart_flush_rx(USART_PORT_WIRELESS);
        rx_errors = errors;
    }

    /* 6. 从无线串口接收数据流并喂入流式协议解析器 */
    n = bsp_usart_receive(USART_PORT_WIRELESS, bytes, sizeof bytes);
    protocol_packet_t packet;
    for (unsigned i = 0; i < n; i++)
        if (protocol_parser_feed(&parser, bytes[i], now, &packet))
        {
            telemetry_payload_t next;
            uint8_t original[23];
            /* 校验遥测载荷格式与数值界限 */
            if (!srv_protocol_unpack(&packet, &next) || !valid_payload(&next))
                continue;
            /* 重新打包全帧并计算 CRC-16 令牌 */
            srv_protocol_pack(packet.seq, &next, original, sizeof original);
            uint16_t token = srv_protocol_token(original, 23);
            /* 喂入统计与去重状态机 */
            stats_packet_result_t result = srv_stats_accept(packet.seq, token, now);
            /* 若为过期的陈旧包，直接抛弃不予回复 */
            if (result == STATS_STALE)
                continue;
            /* 准备 9 字节 ACK 应答帧 */
            srv_protocol_pack_ack(packet.seq, token, ack, sizeof ack);
            ack_pending = true;
            /* 重复包会刷新链路状态并回复 ACK，但不会重复应用或转发数据。 */
            last_packet = now;
            state = RECEIVER_STATE_CONNECTED;
            /* 若为全新递增的数据包，更新当前遥测快照并向上位机转发 */
            if (result == STATS_NEW)
            {
                telemetry = next;
                last_seq = packet.seq;
                published = forward() || published;
            }
        }

    /* 7. 若有准备好的 ACK 应答帧，立即向无线串口发送 */
    if (ack_pending && bsp_usart_transmit(USART_PORT_WIRELESS, ack, sizeof ack))
        ack_pending = false;

    /* 8. 周期向上位机发送统计日志 */
    if (stats_log_pending)
    {
        if (text_mode)
        {
            char line[112];
            int len =
                snprintf(line, sizeof line,
                         "STAT unique=%lu lost=%lu duplicate=%lu Hz10=%u loss10=%u pc_drop=%lu\r\n",
                         (unsigned long)metrics.total_received, (unsigned long)metrics.total_lost,
                         (unsigned long)metrics.total_duplicate, (unsigned)(metrics.freq_hz * 10),
                         (unsigned)(metrics.loss_rate_pct * 10), (unsigned long)pc_drops);
            if (len > 0 && bsp_usart_transmit(
                               USART_PORT_PC_FORWARD, (const uint8_t *)line,
                               (uint16_t)(len < (int)sizeof line ? len : (int)sizeof(line) - 1)))
                stats_log_pending = false;
        }
        /* 在线统计随下一条唯一采样发送，避免用同一采样的副本
         * 再次提高发往 PC 的遥测帧率。 */
        else if (state == RECEIVER_STATE_CONNECTED || published || forward())
        {
            stats_log_pending = false;
        }
    }

    /* 9. OLED 屏幕分片刷新 (更新接收端 UI 画面) */
    bsp_oled_service(now);
    if ((uint32_t)(now - slice_tick) >= slice_interval)
    {
        slice_tick = now;
        slice_interval = (uint8_t)(25 - slice_interval);
        if (bsp_oled_frame_start())
            app_ui_update(&telemetry, metrics.freq_hz, metrics.loss_rate_pct);
        bsp_oled_update_slice();
    }
}

} // invc::receiver 命名空间

/* ==============================================================================
 * C 语言兼容导出接口实现
 * ============================================================================== */

extern "C" void app_receiver_init(void)
{
    invc::receiver::receiver_application().init();
}

extern "C" void app_receiver_task(void)
{
    invc::receiver::receiver_application().task();
}

extern "C" receiver_state_t app_receiver_get_state(void)
{
    return invc::receiver::receiver_application().current_state();
}

extern "C" const telemetry_payload_t *app_receiver_telemetry(void)
{
    return invc::receiver::receiver_application().snapshot();
}

extern "C" uint32_t app_receiver_pc_drops(void)
{
    return invc::receiver::receiver_application().forwarding_drops();
}
