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
#include <string.h>
#include <math.h>
#include <stdio.h>
static sender_view_t view;
static link_tx_t link;
static protocol_parser_t ack_parser;
static uint32_t key_tick, sample_tick, slice_tick, debug_tick, filter_tick, rx_errors;
static uint32_t boot_tick;
static uint32_t tx_base, ack_base, retry_base, expired_base;
static bool imu_was_ready;
static uint8_t slice_interval;
/* Main-context event log queue: keep distinct PRESS/RELEASE/LONG/DOUBLE records. */
static struct
{
    key_msg_t key;
    uint32_t tick;
} event_log[32];
static uint8_t event_head, event_tail;
static uint32_t event_drops;
static void log_key(key_msg_t key, uint32_t now)
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
static void drain_key_log(void)
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
static bool radio_send(const uint8_t *p, uint16_t n)
{
    return bsp_usart_transmit(USART_PORT_WIRELESS, p, n);
}
static bool due(uint32_t now, uint32_t *last, uint32_t period)
{
    if ((uint32_t)(now - *last) < period)
        return false;
    /* Preserve phase without replaying a burst of stale control samples. */
    *last += ((uint32_t)(now - *last) / period) * period;
    return true;
}
void app_sender_recalibrate(void)
{
    srv_imu_filter_calibrate_gyro();
    view.state = APP_STATE_CALIBRATING;
    memset(&view.euler, 0, sizeof(view.euler));
    filter_tick = HAL_GetTick();
}
void app_sender_reset_tx_count(void)
{
    tx_base = link.total_tx;
    ack_base = link.total_acked;
    retry_base = link.total_retry;
    expired_base = link.total_expired;
}
void app_sender_reset_events(void)
{
    view.events = 0;
}
const sender_view_t *app_sender_view(void)
{
    return &view;
}
app_state_t app_sender_get_state(void)
{
    return view.state;
}
void app_sender_init(void)
{
    memset(&view, 0, sizeof(view));
    view.state = APP_STATE_INIT;
    imu_was_ready = false;
    event_head = event_tail = 0;
    event_drops = 0;
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
static int16_t angle10(float a)
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
static void sample_and_send(uint32_t now)
{
    joystick_data_t joy;
    bsp_joystick_get_data(&joy);
    view.telemetry.joy_x_raw = joy.x_mapped;
    view.telemetry.joy_y_raw = joy.y_mapped;
    view.telemetry.joy_x_mv = joy.x_voltage_mv;
    view.telemetry.joy_y_mv = joy.y_voltage_mv;
    bool ready = bsp_imu_get_status() == IMU_STATUS_OK;
    if (ready && !imu_was_ready)
        app_sender_recalibrate();
    imu_was_ready = ready;
    if (ready && bsp_imu_read_raw(&view.raw))
    {
        float dt = (float)(uint32_t)(now - filter_tick) * 0.001f;
        filter_tick = now;
        srv_imu_filter_update(&view.raw, dt, &view.euler);
        srv_imu_filter_raw_angles(&view.raw_angles);
    }
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
    if ((uint32_t)(now - boot_tick) >= LINK_STARTUP_QUIET_MS)
        (void)srv_link_submit(&link, &view.telemetry, now);
}
void app_sender_task(void)
{
    uint32_t now = HAL_GetTick();
    bsp_usart_service(now);
    uint32_t errors = bsp_usart_rx_errors(USART_PORT_WIRELESS);
    if (errors != rx_errors)
    {
        ack_parser.used = 0;
        bsp_usart_flush_rx(USART_PORT_WIRELESS);
        rx_errors = errors;
    }
    uint8_t bytes[64];
    uint16_t n = bsp_usart_receive(USART_PORT_WIRELESS, bytes, sizeof bytes);
    protocol_packet_t p;
    for (uint16_t i = 0; i < n; i++)
        if (protocol_parser_feed(&ack_parser, bytes[i], now, &p))
            (void)srv_link_ack(&link, &p, now);
    srv_link_poll(&link, now);
    if (due(now, &key_tick, 10))
        bsp_key_tick_10ms();
    key_msg_t key;
    while (bsp_key_get_event(&key))
    {
        view.last_key = key;
        view.events++;
        log_key(key, now);
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
        if (key.id == KEY_ID_3 && key.event == KEY_EVENT_LONG_PRESS)
            app_menu_action_long();
    }
    bsp_imu_service(now);
    bsp_joystick_service(now);
    if (due(now, &sample_tick, TELEMETRY_PERIOD_MS))
        sample_and_send(now);
    view.tx = link.total_tx - tx_base;
    view.acked = link.total_acked - ack_base;
    view.retries = link.total_retry - retry_base;
    view.expired = link.total_expired - expired_base;
    view.link_online = link.ever_acked && (uint32_t)(now - link.last_ack_ms) <= LINK_OFFLINE_MS;
    drain_key_log();
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
    bsp_oled_service(now);
    if (due(now, &slice_tick, slice_interval))
    {
        slice_interval = (uint8_t)(25 - slice_interval);
        if (bsp_oled_frame_start())
            app_menu_render();
        bsp_oled_update_slice();
    }
}
