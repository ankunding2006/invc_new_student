#include "srv_stats.h"
#include "system_config.h"
#include <string.h>
static stats_metrics_t metrics;
static uint32_t received, lost, wire, last_ms;
static uint8_t last_seq;
static uint16_t last_token;
static bool synced;
void srv_stats_init(void)
{
    memset(&metrics, 0, sizeof(metrics));
    received = lost = wire = last_ms = 0;
    synced = false;
}
void srv_stats_resync(void)
{
    synced = false;
}
stats_packet_result_t srv_stats_accept(uint8_t seq, uint16_t token, uint32_t now)
{
    wire++;
    if (synced && (uint32_t)(now - last_ms) > LINK_OFFLINE_MS)
        synced = false;
    if (synced)
    {
        uint8_t diff = (uint8_t)(seq - last_seq);
        if (!diff && token == last_token)
        {
            metrics.total_duplicate++;
            last_ms = now;
            return STATS_DUPLICATE;
        }
        if (!diff || diff >= 128)
        {
            metrics.total_stale++;
            return STATS_STALE;
        }
        lost += (uint32_t)(diff - 1);
        metrics.total_lost += (uint32_t)(diff - 1);
    }
    synced = true;
    last_seq = seq;
    last_token = token;
    last_ms = now;
    received++;
    metrics.total_received++;
    return STATS_NEW;
}
void srv_stats_tick(uint32_t elapsed)
{
    if (!elapsed)
        return;
    metrics.freq_hz = (float)received * 1000.0f / (float)elapsed;
    metrics.wire_freq_hz = (float)wire * 1000.0f / (float)elapsed;
    metrics.loss_rate_pct = (received + lost) ? 100.0f * (float)lost / (float)(received + lost) : 0;
    received = lost = wire = 0;
}
void srv_stats_on_packet_received(uint8_t seq)
{
    (void)srv_stats_accept(seq, 0, last_ms);
}
void srv_stats_tick_1000ms(void)
{
    srv_stats_tick(1000);
}
void srv_stats_get_metrics(stats_metrics_t *out)
{
    if (out)
        *out = metrics;
}
