#ifndef SRV_LINK_H
#define SRV_LINK_H
#include "srv_protocol.h"
typedef bool (*link_send_fn)(const uint8_t *, uint16_t);
typedef struct
{
    uint8_t frame[23], seq, retries;
    uint16_t token;
    uint32_t sent_ms, first_ms, total_tx, total_retry, total_acked, total_expired, last_ack_ms;
    bool pending, ever_acked;
    link_send_fn send;
} link_tx_t;
void srv_link_init(link_tx_t *s, link_send_fn send);
bool srv_link_submit(link_tx_t *s, const telemetry_payload_t *data, uint32_t now);
void srv_link_poll(link_tx_t *s, uint32_t now);
bool srv_link_ack(link_tx_t *s, const protocol_packet_t *ack, uint32_t now);
#endif
