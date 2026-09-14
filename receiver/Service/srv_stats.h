#ifndef __SRV_STATS_H
#define __SRV_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 1秒滑动窗口通信统计指标结构体
 * @note 内部遵循 E-SW-02 与 E-COM-07 防御规范：
 *       - 分母为 0 时强制输出 loss_rate_pct = 0.0%，杜绝浮点 NaN；
 *       - 丢包判定采用无符号模 256 环形差值算法，免疫 255 回绕误判。
 */
typedef struct {
    float    freq_hz;           /* 即时接收频率 (Hz) */
    float    loss_rate_pct;     /* 过去 1 秒丢包率百分比 (0.0 ~ 100.0 %) */
    uint32_t total_received;    /* 累计接收有效包总数 */
    uint32_t total_lost;        /* 累计丢失包总数 */
} stats_metrics_t;

void srv_stats_init(void);
void srv_stats_on_packet_received(uint8_t seq_id);
void srv_stats_tick_1000ms(void);
void srv_stats_get_metrics(stats_metrics_t *p_metrics);

#ifdef __cplusplus
}
#endif

#endif /* __SRV_STATS_H */
