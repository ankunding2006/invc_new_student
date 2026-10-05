/**
 * @file sender_application.cpp
 * @brief 发送端应用业务调度实现 (C++17 封装，对接底层 C 驱动与服务)
 */

#include "sender_application.hpp"
extern "C"
{
#include "app_sender.h"
#include "app_menu.h"
#include "srv_link.h"
#include "srv_imu_filter.h"
#include "system_config.h"
#include "bsp_joystick.h"
#include "bsp_key.h"
#include "bsp_switch.h"
#include "bsp_imu.h"
#include "bsp_oled.h"
#include "bsp_usart.h"
#include "main.h"
}
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <type_traits>

namespace invc::sender
{
namespace
{
// 零初始化的静态存储，保持原有 C 启动状态。
static_assert(std::is_trivially_default_constructible_v<SenderApplication>);
static_assert(std::is_trivially_destructible_v<SenderApplication>);
/** @brief 静态全局应用单例实例 (BSS段零初始化，无动态构造开销) */
SenderApplication instance{};
} // 匿名命名空间

/**
 * @brief 获取发送端单例引用
 */
SenderApplication &sender_application() noexcept
{
    return instance;
}

/**
 * @brief 记录按键事件到异步日志队列
 * @param[in] key 按键事件结构体
 * @param[in] now 时间戳
 */
void SenderApplication::log_key(key_msg_t key, uint32_t now)
{
    uint8_t next = (uint8_t)((event_head + 1U) % 32U);
    if (next == event_tail)
    {
        event_drops++;
        return;
    }
    event_log[event_head].key = key;
    event_log[event_head].tick = now;
    event_head = next;
}

/**
 * @brief 异步向串口输出按键日志 (避免阻塞主循环按键响应)
 */
void SenderApplication::drain_key_log(void)
{
    if (event_head == event_tail || !bsp_usart_tx_ready(USART_PORT_DEBUG))
        return;
    static const char *names[] = {"NONE", "PRESS", "RELEASE", "LONG", "DOUBLE"};
    char line[80];
    key_msg_t k = event_log[event_tail].key;
    int n = snprintf(line, sizeof line, "KEY t=%lu K%u %s lost=%lu\r\n",
                     (unsigned long)event_log[event_tail].tick, (unsigned)k.id + 1,
                     names[k.event <= KEY_EVENT_DOUBLE_CLICK ? k.event : 0],
                     (unsigned long)event_drops);
    if (n > 0 && n < (int)sizeof line &&
        bsp_usart_transmit(USART_PORT_DEBUG, (const uint8_t *)line, (uint16_t)n))
        event_tail = (uint8_t)((event_tail + 1U) % 32U);
}

/**
 * @brief 无线底层发送适配包装
 * @param[in] p 数据指针
 * @param[in] n 数据长度
 * @return true 启动发送成功
 */
bool SenderApplication::radio_send(const uint8_t *p, uint16_t n)
{
    return bsp_usart_transmit(USART_PORT_WIRELESS, p, n);
}

/**
 * @brief 保持相位的无累计漂移时间片到达判断
 * @param[in]     now    当前时间戳
 * @param[in,out] last   上一触发周期基准
 * @param[in]     period 期望触发周期 (毫秒)
 * @return true 到达周期; false 未到达
 */
bool SenderApplication::due(uint32_t now, uint32_t *last, uint32_t period)
{
    if ((uint32_t)(now - *last) < period)
        return false;
    /* 保持时间相位，避免一次性重放过期的控制采样。 */
    *last += ((uint32_t)(now - *last) / period) * period;
    return true;
}

/**
 * @brief 触发重新标定陀螺仪并切换应用状态为标定中
 */
void SenderApplication::recalibrate(void)
{
    srv_imu_filter_calibrate_gyro();
    view.state = APP_STATE_CALIBRATING;
    memset(&view.euler, 0, sizeof(view.euler));
    filter_tick = HAL_GetTick();
}

/**
 * @brief 重置 UI 统计的基准计数
 */
void SenderApplication::reset_tx_count(void)
{
    tx_base = link.total_tx;
    ack_base = link.total_acked;
    retry_base = link.total_retry;
    expired_base = link.total_expired;
}

/**
 * @brief 重置累计按键事件计数
 */
void SenderApplication::reset_events(void)
{
    view.events = 0;
}

/**
 * @brief 获取当前应用快照只读指针
 */
const sender_view_t *SenderApplication::snapshot(void)
{
    return &view;
}

/**
 * @brief 获取当前系统状态
 */
app_state_t SenderApplication::current_state(void)
{
    return view.state;
}

/**
 * @brief 初始化发送端系统全部驱动与服务
 */
void SenderApplication::init(void)
{
    memset(&view, 0, sizeof(view));
    view.state = APP_STATE_INIT;
    imu_was_ready = false;
    event_head = event_tail = 0;
    event_drops = 0;
    // 初始化各硬件模块和通信服务。
    bsp_usart_init();
    bsp_joystick_init();
    bsp_key_init();
    bsp_switch_init();
    (void)bsp_imu_init();
    bsp_oled_init();
    srv_imu_filter_init(50);
    srv_link_init(&link, radio_send);
    protocol_parser_init(&ack_parser);
    app_menu_init();
    key_tick = sample_tick = slice_tick = debug_tick = filter_tick = HAL_GetTick();
    slice_interval = 12;
    boot_tick = HAL_GetTick();
    rx_errors = 0;
    tx_base = ack_base = retry_base = expired_base = 0;
}

/**
 * @brief 角度缩放与限幅处理 (转为 0.1 度为单位的整数)
 * @param[in] a 浮点角度 (度)
 * @return 0.1 度整数 (-32768 ~ +32767)
 */
int16_t SenderApplication::angle10(float a)
{
    if (!isfinite(a))
        return 0;
    float v = a * 10;
    if (v > 32767)
        v = 32767;
    if (v < -32768)
        v = -32768;
    return (int16_t)(v + (v < 0 ? -0.5f : 0.5f));
}

/**
 * @brief 传感器采样、姿态解算与无线数据帧提交
 * @param[in] now 当前时间戳
 */
void SenderApplication::sample_and_send(uint32_t now)
{
    /* 1. 采集摇杆数据 */
    joystick_data_t joy;
    bsp_joystick_get_data(&joy);
    view.telemetry.joy_x_raw = joy.x_mapped;
    view.telemetry.joy_y_raw = joy.y_mapped;
    view.telemetry.joy_x_mv = joy.x_voltage_mv;
    view.telemetry.joy_y_mv = joy.y_voltage_mv;

    /* 2. 采集 IMU 数据与解算 */
    bool ready = bsp_imu_get_status() == IMU_STATUS_OK;
    /* 若 IMU 从未就绪恢复为就绪，自动重新校准陀螺仪 */
    if (ready && !imu_was_ready)
        recalibrate();
    imu_was_ready = ready;
    if (ready && bsp_imu_read_raw(&view.raw))
    {
        float dt = (float)(uint32_t)(now - filter_tick) * 0.001f;
        filter_tick = now;
        srv_imu_filter_update(&view.raw, dt, &view.euler);
        srv_imu_filter_raw_angles(&view.raw_angles);
    }

    /* 3. 判定当前系统健康状态与降级状态 */
    ready = bsp_imu_get_status() == IMU_STATUS_OK;
    if (!ready)
    {
        memset(&view.euler, 0, sizeof(view.euler));
        filter_tick = now;
        view.state =
            bsp_imu_get_status() == IMU_STATUS_STARTING ? APP_STATE_INIT : APP_STATE_FAULT_DEGRADED;
    }
    else if (!srv_imu_filter_is_calibrated())
        view.state = APP_STATE_CALIBRATING;
    else
        view.state = (bsp_joystick_ready() && bsp_oled_ready()) ? APP_STATE_NORMAL
                                                                : APP_STATE_FAULT_DEGRADED;

    // 汇总传感器、按键和显示模块状态，附加到遥测标志位中。
    uint8_t flags = bsp_key_get_mask();
    if (!ready)
        flags |= TELEMETRY_IMU_FAULT;
    if (ready && !srv_imu_filter_is_calibrated())
        flags |= TELEMETRY_CALIBRATING;
    if (!bsp_joystick_ready())
        flags |= TELEMETRY_ADC_FAULT;
    if (!bsp_oled_ready())
        flags |= TELEMETRY_OLED_FAULT;
    view.telemetry.key_mask = flags;
    view.telemetry.switch_mask = bsp_switch_get_mask();
    view.telemetry.pitch_cd = angle10(view.euler.pitch);
    view.telemetry.roll_cd = angle10(view.euler.roll);
    view.telemetry.yaw_cd = angle10(view.euler.yaw);

    /* 4. 开机静默时间 (1500ms) 过后，才向无线链路层提交遥测帧 */
    if ((uint32_t)(now - boot_tick) >= LINK_STARTUP_QUIET_MS)
        (void)srv_link_submit(&link, &view.telemetry, now);
}

/**
 * @brief 发送端应用主循环任务函数
 */
void SenderApplication::task(void)
{
    uint32_t now = HAL_GetTick();

    /* 1. 串口驱动后台服务与错误自愈 */
    bsp_usart_service(now);
    uint32_t errors = bsp_usart_rx_errors(USART_PORT_WIRELESS);
    if (errors != rx_errors)
    {
        ack_parser.used = 0;
        bsp_usart_flush_rx(USART_PORT_WIRELESS);
        rx_errors = errors;
    }

    /* 2. 接收无线应答包并驱动链路状态机 */
    uint8_t bytes[64];
    uint16_t n = bsp_usart_receive(USART_PORT_WIRELESS, bytes, sizeof bytes);
    protocol_packet_t p;
    // 先解析无线应答，再驱动链路状态机。
    for (uint16_t i = 0; i < n; i++)
        if (protocol_parser_feed(&ack_parser, bytes[i], now, &p))
            (void)srv_link_ack(&link, &p, now);
    srv_link_poll(&link, now);

    /* 3. 10ms 周期按键扫描与事件分发处理 */
    if (due(now, &key_tick, 10))
        bsp_key_tick_10ms();
    key_msg_t key;
    while (bsp_key_get_event(&key))
    {
        view.last_key = key;
        view.events++;
        log_key(key, now);
        /* 按下短按事件: 驱动菜单导航 (K1上, K2下, K3进, K4退) */
        if (key.event == KEY_EVENT_PRESS)
        {
            switch (key.id)
            {
            case KEY_ID_1:
                app_menu_navigate_up();
                break;
            case KEY_ID_2:
                app_menu_navigate_down();
                break;
            case KEY_ID_3:
                app_menu_action_enter();
                break;
            case KEY_ID_4:
                app_menu_action_back();
                break;
            default:
                break;
            }
        }
        /* K3 长按事件: 触发特殊动作 (如姿态重新校准) */
        if (key.id == KEY_ID_3 && key.event == KEY_EVENT_LONG_PRESS)
            app_menu_action_long();
    }

    /* 4. IMU 与摇杆驱动维护任务 */
    bsp_imu_service(now);
    bsp_joystick_service(now);

    /* 5. 20ms (50Hz) 传感器采集与无线发射 */
    if (due(now, &sample_tick, TELEMETRY_PERIOD_MS))
        sample_and_send(now);

    /* 6. 更新视图层统计指标与链路在线状态 */
    view.tx = link.total_tx - tx_base;
    view.acked = link.total_acked - ack_base;
    view.retries = link.total_retry - retry_base;
    view.expired = link.total_expired - expired_base;
    view.link_online = link.ever_acked && (uint32_t)(now - link.last_ack_ms) <= LINK_OFFLINE_MS;

    /* 7. 输出按键调试日志 */
    drain_key_log();

    /* 8. 100ms 周期串口文本调试帧输出 */
    if (due(now, &debug_tick, 100) && event_head == event_tail &&
        bsp_usart_tx_ready(USART_PORT_DEBUG))
    {
        bsp_usart_printf(
            USART_PORT_DEBUG,
            "S x=%d y=%d mv=%u,%u key=%X sw=%X a=%d,%d,%d g=%d,%d,%d raw10=%d,%d out10=%d,%d,%d "
            "st=%u ack=%lu retry=%lu\r\n",
            view.telemetry.joy_x_raw, view.telemetry.joy_y_raw, view.telemetry.joy_x_mv,
            view.telemetry.joy_y_mv, view.telemetry.key_mask & 15, view.telemetry.switch_mask,
            view.raw.ax, view.raw.ay, view.raw.az, view.raw.gx, view.raw.gy, view.raw.gz,
            angle10(view.raw_angles.pitch), angle10(view.raw_angles.roll), view.telemetry.pitch_cd,
            view.telemetry.roll_cd, view.telemetry.yaw_cd, (unsigned)view.state,
            (unsigned long)link.total_acked, (unsigned long)link.total_retry);
    }

    /* 9. OLED 屏幕分片刷新 (交替 12ms/13ms 间隔，8 次分片合成一完整帧) */
    bsp_oled_service(now);
    if (due(now, &slice_tick, slice_interval))
    {
        slice_interval = (uint8_t)(25 - slice_interval);
        if (bsp_oled_frame_start())
            app_menu_render();
        bsp_oled_update_slice();
    }
}

} // invc::sender 命名空间

/* ==============================================================================
 * C 语言兼容导出接口实现
 * ============================================================================== */

extern "C" void app_sender_init(void)
{
    invc::sender::sender_application().init();
}

extern "C" void app_sender_task(void)
{
    invc::sender::sender_application().task();
}

extern "C" app_state_t app_sender_get_state(void)
{
    return invc::sender::sender_application().current_state();
}

extern "C" const sender_view_t *app_sender_view(void)
{
    return invc::sender::sender_application().snapshot();
}

extern "C" void app_sender_recalibrate(void)
{
    invc::sender::sender_application().recalibrate();
}

extern "C" void app_sender_reset_tx_count(void)
{
    invc::sender::sender_application().reset_tx_count();
}

extern "C" void app_sender_reset_events(void)
{
    invc::sender::sender_application().reset_events();
}
