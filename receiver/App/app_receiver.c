#include "app_receiver.h"
#include "app_ui.h"
#include "srv_protocol_parser.h"
#include "srv_stats.h"
#include "bsp_oled.h"
#include "bsp_usart.h"

static receiver_state_t s_receiver_state = RECEIVER_STATE_WAIT_SYNC;

static void on_frame_received(uint8_t seq_id, const telemetry_payload_t *p_payload)
{
    /* 统计模块计包 */
    srv_stats_on_packet_received(seq_id);
    s_receiver_state = RECEIVER_STATE_CONNECTED;

    /* 转发至 PC 电脑端 (VOFA+) */
    bsp_usart_forward_packet((const uint8_t *)p_payload, sizeof(telemetry_payload_t));
}

void app_receiver_init(void)
{
    s_receiver_state = RECEIVER_STATE_WAIT_SYNC;

    /* 初始化外设驱动 */
    bsp_oled_init();
    bsp_usart_init();

    /* 初始化服务层 */
    srv_protocol_parser_init(on_frame_received);
    srv_stats_init();

    /* 初始化 UI 界面 */
    app_ui_init();
}

void app_receiver_task(void)
{
    /* 接收端主任务循环骨架 */
}

receiver_state_t app_receiver_get_state(void)
{
    return s_receiver_state;
}
