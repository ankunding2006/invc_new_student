#ifndef SRV_STATS_H
#define SRV_STATS_H
#include <stdint.h>
#include <stdbool.h>
typedef struct
{
    float freq_hz, loss_rate_pct;
    uint32_t total_received, total_lost, total_duplicate, total_stale;
    float wire_freq_hz;
} stats_metrics_t;
typedef enum
{
    STATS_NEW,
    STATS_DUPLICATE,
    STATS_STALE
} stats_packet_result_t;
void srv_stats_init(void);
stats_packet_result_t srv_stats_accept(uint8_t seq, uint16_t token, uint32_t now);
void srv_stats_resync(void);
void srv_stats_tick(uint32_t elapsed_ms);
void srv_stats_on_packet_received(uint8_t seq);
void srv_stats_tick_1000ms(void);
void srv_stats_get_metrics(stats_metrics_t *out);
#endif
