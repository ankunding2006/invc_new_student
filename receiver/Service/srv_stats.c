#include "srv_stats.h"
#include <stddef.h>

#define CLAMP(x, min, max)  ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

static stats_metrics_t s_metrics = {0.0f, 0.0f, 0, 0};
static uint32_t s_window_received = 0;
static uint32_t s_window_lost = 0;
static uint8_t  s_last_seq = 0;
static bool     s_has_first_packet = false;

void srv_stats_init(void)
{
    s_metrics.freq_hz = 0.0f;
    s_metrics.loss_rate_pct = 0.0f;
    s_metrics.total_received = 0;
    s_metrics.total_lost = 0;
    s_window_received = 0;
    s_window_lost = 0;
    s_has_first_packet = false;
}

void srv_stats_on_packet_received(uint8_t seq_id)
{
    s_metrics.total_received++;
    s_window_received++;

    if (!s_has_first_packet) {
        s_last_seq = seq_id;
        s_has_first_packet = true;
        return;
    }

    /* E-COM-07: 模 256 环形差值算法防 255 回绕误判 */
    uint8_t diff = (uint8_t)(seq_id - s_last_seq);
    if (diff > 1) {
        uint8_t lost = (uint8_t)(diff - 1);
        s_metrics.total_lost += lost;
        s_window_lost += lost;
    }
    s_last_seq = seq_id;
}

void srv_stats_tick_1000ms(void)
{
    /* E-SW-02: 1000ms 窗口结算，除零硬防御 */
    uint32_t total_in_window = s_window_received + s_window_lost;
    s_metrics.freq_hz = (float)s_window_received;

    if (total_in_window == 0) {
        s_metrics.loss_rate_pct = 0.0f; /* 分母为零安全保护，杜绝产生浮点 NaN */
    } else {
        s_metrics.loss_rate_pct = ((float)s_window_lost / (float)total_in_window) * 100.0f;
        s_metrics.loss_rate_pct = CLAMP(s_metrics.loss_rate_pct, 0.0f, 100.0f);
    }

    /* 结算后复位当前秒级窗口计数值 */
    s_window_received = 0;
    s_window_lost = 0;
}

void srv_stats_get_metrics(stats_metrics_t *p_metrics)
{
    if (p_metrics != NULL) {
        *p_metrics = s_metrics;
    }
}
