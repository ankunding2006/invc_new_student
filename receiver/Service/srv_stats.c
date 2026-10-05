/**
 * @file srv_stats.c
 * @brief 接收端包去重、丢包推算与通信指标计算实现
 */

#include "srv_stats.h"
#include "system_config.h"
#include <string.h>

/** @brief 统计指标聚合数据 */
static stats_metrics_t metrics;
/** @brief 窗口内有效接收数、推算丢包数、物理空中包总数、最近接收时间戳 */
static uint32_t received, lost, wire, last_ms;
/** @brief 最近一次有效包的序列号 */
static uint8_t last_seq;
/** @brief 最近一次有效包的 CRC-16 令牌 */
static uint16_t last_token;
/** @brief 是否已与发送端建立基准序号同步 */
static bool synced;

/**
 * @brief 初始化统计服务内部所有计数与状态
 */
void srv_stats_init(void)
{
    memset(&metrics, 0, sizeof(metrics));
    received = lost = wire = last_ms = 0;
    synced = false;
}

/**
 * @brief 标记脱机并需要重新同步序号
 */
void srv_stats_resync(void)
{
    synced = false;
}

/**
 * @brief 判别新到达的协议包 (处理序号回绕、重复包与丢包推算)
 * @param[in] seq   包序列号 (0~255)
 * @param[in] token 16位 CRC 令牌
 * @param[in] now   当前毫秒时间戳
 * @return 去重判定结果
 */
stats_packet_result_t srv_stats_accept(uint8_t seq, uint16_t token, uint32_t now)
{
    wire++; /* 空中接收到的物理帧总数自增 */
    /* 若超过离线超时时间 (LINK_OFFLINE_MS=1000ms)，判定连接已断开，清除同步标志以重新对齐基准 */
    if (synced && (uint32_t)(now - last_ms) > LINK_OFFLINE_MS)
        synced = false;
    if (synced)
    {
        /* 利用 8 位无符号整数减法天然支持 0~255 模 256 序号回绕 (例如 0 - 255 = 1) */
        uint8_t diff = (uint8_t)(seq - last_seq);
        /* 1. 重复包判定: 序号差为 0 且 CRC 令牌一致 */
        if (!diff && token == last_token)
        {
            metrics.total_duplicate++;
            last_ms = now;
            return STATS_DUPLICATE;
        }
        /* 2. 过期/历史包判定: 序号差为 0 但令牌不同，或者序号差 >= 128 (判定为落后半周期的旧包) */
        if (!diff || diff >= 128)
        {
            metrics.total_stale++;
            return STATS_STALE;
        }
        /* 3. 正常向前推进: 若序号跳跃 (diff > 1)，说明中间有 diff - 1 个包在信道中丢失 */
        lost += (uint32_t)(diff - 1);
        metrics.total_lost += (uint32_t)(diff - 1);
    }
    /* 建立或刷新基准序号与时间戳 */
    synced = true;
    last_seq = seq;
    last_token = token;
    last_ms = now;
    received++;
    metrics.total_received++;
    return STATS_NEW;
}

/**
 * @brief 周期结算通信频率与丢包率
 * @param[in] elapsed 经历的毫秒数
 */
void srv_stats_tick(uint32_t elapsed)
{
    if (!elapsed)
        return;
    /* 计算每秒有效接收频率 (Hz) */
    metrics.freq_hz = (float)received * 1000.0f / (float)elapsed;
    /* 计算每秒物理空中包接收频率 (Hz) */
    metrics.wire_freq_hz = (float)wire * 1000.0f / (float)elapsed;
    /* 计算当前窗口丢包率百分比: lost / (received + lost) * 100% */
    metrics.loss_rate_pct = (received + lost) ? 100.0f * (float)lost / (float)(received + lost) : 0;
    /* 清零周期窗口临时计数器 */
    received = lost = wire = 0;
}

/**
 * @brief 单包接收统计适配接口
 * @param[in] seq 包序号
 */
void srv_stats_on_packet_received(uint8_t seq)
{
    (void)srv_stats_accept(seq, 0, last_ms);
}

/**
 * @brief 1000ms 周期统计结算
 */
void srv_stats_tick_1000ms(void)
{
    srv_stats_tick(1000);
}

/**
 * @brief 获取统计指标数据快照
 * @param[out] out 接收指针
 */
void srv_stats_get_metrics(stats_metrics_t *out)
{
    if (out)
        *out = metrics;
}
