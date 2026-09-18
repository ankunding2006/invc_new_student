#include "srv_link.h"
#include "system_config.h"
#include <string.h>
void srv_link_init(link_tx_t *s, link_send_fn send)
{
    if (s)
    {
        memset(s, 0, sizeof(*s));
        s->send = send;
    }
}
bool srv_link_submit(link_tx_t *s, const telemetry_payload_t *p, uint32_t now)
{
    if (!s || !p || s->pending || !s->send)
        return false;
    srv_protocol_pack(s->seq, p, s->frame, sizeof(s->frame));
    if (!s->send(s->frame, 23))
        return false;
    s->token = srv_protocol_token(s->frame, 23);
    s->pending = true;
    s->retries = 0;
    s->sent_ms = s->first_ms = now;
    s->total_tx++;
    return true;
}
void srv_link_poll(link_tx_t *s, uint32_t now)
{
    if (!s || !s->pending)
        return;
    if ((uint32_t)(now - s->first_ms) >= LINK_ACK_TIMEOUT_MS * (LINK_MAX_RETRIES + 1U))
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    if ((uint32_t)(now - s->sent_ms) < LINK_ACK_TIMEOUT_MS)
        return;
    if (s->retries >= LINK_MAX_RETRIES)
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    /* Absolute expiry prevents a busy UART from retaining an old sample forever. */
    if ((uint32_t)(now - s->sent_ms) >= LINK_ACK_TIMEOUT_MS * 2U)
    {
        s->pending = false;
        s->seq++;
        s->total_expired++;
        return;
    }
    if (s->send && s->send(s->frame, 23))
    {
        s->sent_ms = now;
        s->retries++;
        s->total_tx++;
        s->total_retry++;
    }
}
bool srv_link_ack(link_tx_t *s, const protocol_packet_t *p, uint32_t now)
{
    if (!s || !p || !s->pending || p->cmd != PROTOCOL_CMD_ACK || p->len != 2 || p->seq != s->seq)
        return false;
    uint16_t token = (uint16_t)(p->payload[0] | ((uint16_t)p->payload[1] << 8));
    if (token != s->token)
        return false;
    s->pending = false;
    s->seq++;
    s->total_acked++;
    s->last_ack_ms = now;
    s->ever_acked = true;
    return true;
}
