#include "app_receiver.h"
#include "app_ui.h"
#include "srv_pc.h"
#include "system_config.h"
#include "bsp_oled.h"
#include "bsp_usart.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
static receiver_state_t state;
static protocol_parser_t parser;
static telemetry_payload_t telemetry;
static stats_metrics_t metrics;
static uint32_t last_packet, stats_tick, slice_tick, rx_errors, pc_drops;
static uint8_t slice_interval, last_seq, ack[9];
static bool ack_pending, text_mode, stats_log_pending;
const telemetry_payload_t *app_receiver_telemetry(void)
{
    return &telemetry;
}
receiver_state_t app_receiver_get_state(void)
{
    return state;
}
uint32_t app_receiver_pc_drops(void)
{
    return pc_drops;
}
static bool forward(void)
{
    uint8_t output[192];
    uint16_t len;
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
    else
        len = srv_pc_pack(&telemetry, metrics.freq_hz, metrics.loss_rate_pct, metrics.wire_freq_hz,
                          state == RECEIVER_STATE_CONNECTED, output, sizeof output);
    if (len && !bsp_usart_transmit(USART_PORT_PC_FORWARD, output, len))
    {
        pc_drops++;
        return false;
    }
    return len != 0;
}
void app_receiver_init(void)
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
static bool valid_payload(const telemetry_payload_t *p)
{
    return p->joy_x_raw >= -1000 && p->joy_x_raw <= 1000 && p->joy_y_raw >= -1000 &&
           p->joy_y_raw <= 1000 && p->joy_x_mv <= 3300 && p->joy_y_mv <= 3300 &&
           p->switch_mask <= 3 && p->pitch_cd >= -1800 && p->pitch_cd <= 1800 &&
           p->roll_cd >= -1800 && p->roll_cd <= 1800 && p->yaw_cd >= 0 && p->yaw_cd <= 3600;
}
void app_receiver_task(void)
{
    bool published = false;
    uint32_t now = HAL_GetTick();
    bsp_usart_service(now);
    if (state == RECEIVER_STATE_CONNECTED && (uint32_t)(now - last_packet) > LINK_OFFLINE_MS)
    {
        state = RECEIVER_STATE_OFFLINE;
        srv_stats_resync();
        metrics.freq_hz = metrics.wire_freq_hz = 0;
        published = forward();
    }
    /* Close the previous window before accounting newly consumed packets. */
    if ((uint32_t)(now - stats_tick) >= 1000)
    {
        srv_stats_tick(now - stats_tick);
        stats_tick = now;
        srv_stats_get_metrics(&metrics);
        if (state != RECEIVER_STATE_CONNECTED)
            metrics.freq_hz = metrics.wire_freq_hz = 0;
        stats_log_pending = true;
    }
    uint8_t bytes[96];
    uint16_t n = bsp_usart_receive(USART_PORT_PC_FORWARD, bytes, sizeof bytes);
    for (unsigned i = 0; i < n; i++)
    {
        if (bytes[i] == 'T' || bytes[i] == 't')
            text_mode = true;
        else if (bytes[i] == 'F' || bytes[i] == 'f')
            text_mode = false;
    }
    uint32_t errors = bsp_usart_rx_errors(USART_PORT_WIRELESS);
    if (errors != rx_errors)
    {
        parser.used = 0;
        bsp_usart_flush_rx(USART_PORT_WIRELESS);
        rx_errors = errors;
    }
    n = bsp_usart_receive(USART_PORT_WIRELESS, bytes, sizeof bytes);
    protocol_packet_t packet;
    for (unsigned i = 0; i < n; i++)
        if (protocol_parser_feed(&parser, bytes[i], now, &packet))
        {
            telemetry_payload_t next;
            uint8_t original[23];
            if (!srv_protocol_unpack(&packet, &next) || !valid_payload(&next))
                continue;
            srv_protocol_pack(packet.seq, &next, original, sizeof original);
            uint16_t token = srv_protocol_token(original, 23);
            stats_packet_result_t result = srv_stats_accept(packet.seq, token, now);
            if (result == STATS_STALE)
                continue;
            srv_protocol_pack_ack(packet.seq, token, ack, sizeof ack);
            ack_pending = true;
            /* Duplicates renew link presence and ACK, but never reapply/forward data. */
            last_packet = now;
            state = RECEIVER_STATE_CONNECTED;
            if (result == STATS_NEW)
            {
                telemetry = next;
                last_seq = packet.seq;
                published = forward() || published;
            }
        }
    if (ack_pending && bsp_usart_transmit(USART_PORT_WIRELESS, ack, sizeof ack))
        ack_pending = false;
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
        /* Online statistics ride the next unique sample; do not inflate the
         * PC telemetry frame rate with a second copy of the same sample. */
        else if (state == RECEIVER_STATE_CONNECTED || published || forward())
        {
            stats_log_pending = false;
        }
    }
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
