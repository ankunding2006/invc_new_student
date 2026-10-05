#ifndef SRV_STATS_H
#define SRV_STATS_H

/**
 * @file srv_stats.h
 * @brief 接收端通信统计与包去重服务 (频率计算、丢包率统计、重复包过滤)
 */

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 通信统计指标结构体
 */
typedef struct
{
    float freq_hz, loss_rate_pct;                                     /**< 实际有效帧接收频率 (Hz)、估算丢包率百分比 (0~100%) */
    uint32_t total_received, total_lost, total_duplicate, total_stale;/**< 累计接收有效包、累计推算丢包、累计重复包、累计过期包 */
    float wire_freq_hz;                                               /**< 包含重复帧在内的物理层空中包接收频率 (Hz) */
} stats_metrics_t;

/**
 * @brief 接收包去重判断结果枚举
 */
typedef enum
{
    STATS_NEW,        /**< 新到达的递增有效数据包 (应更新遥测并回复 ACK) */
    STATS_DUPLICATE,  /**< 重复包: 序号与 CRC 令牌均与上一包完全相同 (重传引起，应重新回复 ACK 但不重复转发) */
    STATS_STALE       /**< 过期包: 序号回退或异常乱序包 (直接忽略丢弃) */
} stats_packet_result_t;

/**
 * @brief 初始化统计服务状态
 */
void srv_stats_init(void);

/**
 * @brief 接收并判别新到达的协议包 (执行去重、丢包推算与序号追踪)
 * @param[in] seq   包序号 (0~255)
 * @param[in] token 数据帧 CRC-16 令牌
 * @param[in] now   当前时间戳 (毫秒)
 * @return 去重判定结果: STATS_NEW, STATS_DUPLICATE 或 STATS_STALE
 */
stats_packet_result_t srv_stats_accept(uint8_t seq, uint16_t token, uint32_t now);

/**
 * @brief 触发断链重新同步 (重置同步标志，下一包无论序号为何均作为基准重新接收)
 */
void srv_stats_resync(void);

/**
 * @brief 统计周期窗口结算任务 (计算过去一段时间内的频率与丢包率)
 * @param[in] elapsed_ms 距上次结算经历的实际毫秒数 (通常为 1000ms)
 */
void srv_stats_tick(uint32_t elapsed_ms);

/**
 * @brief 简易包接收统计入口
 * @param[in] seq 包序号
 */
void srv_stats_on_packet_received(uint8_t seq);

/**
 * @brief 1000ms 周期结算接口
 */
void srv_stats_tick_1000ms(void);

/**
 * @brief 获取当前最新的统计指标数据快照
 * @param[out] out 接收统计指标的结构体指针
 */
void srv_stats_get_metrics(stats_metrics_t *out);

#endif
