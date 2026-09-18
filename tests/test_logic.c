#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "srv_protocol.h"
#include "srv_link.h"
#include "srv_input.h"
#include "srv_imu_filter.h"
#include "srv_stats.h"
#include "system_config.h"
static unsigned sent;
static bool busy;
static bool send_frame(const uint8_t *p, uint16_t n)
{
    assert(p && n == 23);
    if (busy)
        return false;
    sent++;
    return true;
}
static telemetry_payload_t sample = {-1000, 1000, 3300, 0, 15, 3, -1800, 1800, 3600};
static bool feed(protocol_parser_t *p, const uint8_t *data, unsigned n, uint32_t now,
                 protocol_packet_t *out)
{
    bool found = false;
    for (unsigned i = 0; i < n; i++)
        if (protocol_parser_feed(p, data[i], now, out))
            found = true;
    return found;
}
static void protocols(void)
{
    uint8_t frame[25];
    memset(frame, 0xCC, sizeof(frame));
    assert(srv_protocol_pack(254, &sample, frame + 1, 22) == 0 && frame[1] == 0xCC);
    assert(srv_protocol_pack(254, &sample, frame + 1, 23) == 23 && frame[0] == 0xCC &&
           frame[24] == 0xCC);
    const uint8_t expected[] = {0xAA, 0x55, 254, 16,   1,    0x18, 0xFC, 0xE8, 3,    0xE4, 0x0C, 0,
                                0,    15,   3,   0xF8, 0xF8, 8,    7,    0x10, 0x0E, 0x2D, 0x0D};
    assert(!memcmp(expected, frame + 1, 23));
    assert(srv_protocol_token((const uint8_t *)"123456789", 9) == 0x29B1);
    for (unsigned split = 0; split <= 23; split++)
    {
        protocol_parser_t p;
        protocol_packet_t pkt;
        protocol_parser_init(&p);
        bool found = feed(&p, expected, split, 1, &pkt);
        found = feed(&p, expected + split, 23 - split, 2, &pkt) || found;
        telemetry_payload_t decoded;
        assert(found && srv_protocol_unpack(&pkt, &decoded));
        assert(!memcmp(&sample, &decoded, sizeof sample));
    }
    for (unsigned corrupt = 0; corrupt < 23; corrupt++)
    {
        uint8_t bad[23];
        memcpy(bad, expected, 23);
        bad[corrupt] ^= 0x80;
        protocol_parser_t p;
        protocol_packet_t pkt;
        protocol_parser_init(&p);
        assert(!feed(&p, bad, 23, 1, &pkt));
        assert(feed(&p, expected, 23, 2, &pkt));
    }
    protocol_parser_t p;
    protocol_packet_t pkt;
    protocol_parser_init(&p);
    uint8_t prefix[] = {0xAA, 0xAA, 0x55, 7, 16, 1};
    feed(&p, prefix, sizeof prefix, 1, &pkt);
    assert(feed(&p, expected, 23, 2, &pkt));
    assert(feed(&p, expected, 23, 3, &pkt));
    protocol_parser_init(&p);
    feed(&p, expected, 10, 10, &pkt);
    assert(feed(&p, expected, 23, 200, &pkt));
    assert(p.errors > 0);
    uint8_t ack[9];
    srv_protocol_pack_ack(254, srv_protocol_token(expected, 23), ack, 9);
    assert(feed(&p, ack, 9, 201, &pkt) && pkt.cmd == PROTOCOL_CMD_ACK);
    /* Deterministic random noise + a clean gap must always recover. */
    uint32_t rng = 1;
    for (unsigned trial = 0; trial < 1000; trial++)
    {
        for (unsigned i = 0; i < 57; i++)
        {
            rng = rng * 1664525U + 1013904223U;
            (void)protocol_parser_feed(&p, (uint8_t)(rng >> 24), trial * 200, &pkt);
        }
        assert(feed(&p, expected, 23, trial * 200 + 101, &pkt));
    }
}
static void reliable_link(void)
{
    link_tx_t link;
    srv_link_init(&link, send_frame);
    sent = 0;
    busy = true;
    assert(!srv_link_submit(&link, &sample, 0) && !link.pending);
    busy = false;
    assert(srv_link_submit(&link, &sample, 0) && sent == 1);
    assert(!srv_link_submit(&link, &sample, 20));
    srv_link_poll(&link, 79);
    assert(sent == 1);
    srv_link_poll(&link, 80);
    assert(sent == 2);
    protocol_packet_t ack = {0, PROTOCOL_CMD_ACK, 2, {0}};
    ack.payload[0] = (uint8_t)link.token;
    ack.payload[1] = (uint8_t)(link.token >> 8);
    ack.seq = 1;
    assert(!srv_link_ack(&link, &ack, 81));
    ack.seq = 0;
    assert(srv_link_ack(&link, &ack, 82) && link.seq == 1 && !link.pending);
    assert(!srv_link_ack(&link, &ack, 83));
    assert(srv_link_submit(&link, &sample, UINT32_MAX - 39));
    srv_link_poll(&link, 40);
    srv_link_poll(&link, 120);
    srv_link_poll(&link, 200);
    assert(!link.pending && link.total_expired == 1 && link.total_retry == 3);
    /* A late retry must not extend the total age past 240 ms. */
    srv_link_init(&link, send_frame);
    assert(srv_link_submit(&link, &sample, 0));
    busy = true;
    srv_link_poll(&link, 80);
    busy = false;
    srv_link_poll(&link, 159);
    srv_link_poll(&link, 239);
    srv_link_poll(&link, 240);
    assert(!link.pending && link.total_expired == 1);
    srv_stats_init();
    assert(srv_stats_accept(254, 1, 1) == STATS_NEW);
    assert(srv_stats_accept(255, 2, 2) == STATS_NEW);
    assert(srv_stats_accept(0, 3, 3) == STATS_NEW);
    assert(srv_stats_accept(0, 3, 4) == STATS_DUPLICATE);
    assert(srv_stats_accept(255, 2, 5) == STATS_STALE);
    assert(srv_stats_accept(2, 4, 6) == STATS_NEW);
    srv_stats_tick(2000);
    stats_metrics_t m;
    srv_stats_get_metrics(&m);
    assert(m.freq_hz == 2 && m.wire_freq_hz == 3 && m.total_lost == 1 && m.loss_rate_pct == 20);
    assert(srv_stats_accept(200, 9, 2000) == STATS_NEW);
    srv_stats_tick(1000);
    srv_stats_tick(1000);
    srv_stats_get_metrics(&m);
    assert(m.freq_hz == 0 && m.loss_rate_pct == 0);
}
static key_event_t event(void)
{
    key_msg_t m;
    return srv_keys_pop(&m) ? m.event : KEY_EVENT_NONE;
}
static void inputs(void)
{
    srv_keys_init();
    srv_keys_feed(1, 0);
    srv_keys_feed(0, 10);
    srv_keys_feed(0, 40);
    assert(event() == KEY_EVENT_NONE);
    srv_keys_feed(1, 50);
    srv_keys_feed(1, 70);
    assert(event() == KEY_EVENT_PRESS);
    srv_keys_feed(0, 100);
    srv_keys_feed(0, 120);
    assert(event() == KEY_EVENT_RELEASE);
    srv_keys_feed(1, 200);
    srv_keys_feed(1, 220);
    assert(event() == KEY_EVENT_PRESS && event() == KEY_EVENT_DOUBLE_CLICK);
    srv_keys_feed(1, 2219);
    assert(event() == KEY_EVENT_NONE);
    srv_keys_feed(1, 2220);
    assert(event() == KEY_EVENT_LONG_PRESS);
    srv_keys_feed(1, 4000);
    assert(event() == KEY_EVENT_NONE);
    srv_keys_feed(0, 4001);
    srv_keys_feed(0, 4021);
    assert(event() == KEY_EVENT_RELEASE);
    srv_keys_init();
    srv_keys_feed(1, UINT32_MAX - 9);
    srv_keys_feed(1, 10);
    assert(event() == KEY_EVENT_PRESS);
    srv_keys_init();
    for (unsigned t = 0; t < 4000; t += 30)
        srv_keys_feed((t / 30) & 1, t); /* unstable edges never accepted */
    assert(srv_keys_overflows() == 0);
    srv_keys_init();
    for (unsigned i = 0; i < 3; i++)
    {
        srv_keys_feed(1, 100 * i);
        srv_keys_feed(1, 100 * i + 20);
        assert(event() == KEY_EVENT_PRESS);
        assert(event() == (i == 1 ? KEY_EVENT_DOUBLE_CLICK : KEY_EVENT_NONE));
        srv_keys_feed(0, 100 * i + 40);
        srv_keys_feed(0, 100 * i + 60);
        assert(event() == KEY_EVENT_RELEASE);
    }
    assert(srv_joystick_map(0, 1650) == -1000 && srv_joystick_map(3300, 1650) == 1000);
    for (unsigned mv = 1570; mv <= 1730; mv++)
        assert(srv_joystick_map((uint16_t)mv, 1650) == 0);
    for (unsigned mv = 0; mv < 3300; mv++)
        assert(srv_joystick_map((uint16_t)mv, 1650) <= srv_joystick_map((uint16_t)(mv + 1), 1650));
}
static void imu(void)
{
    imu_raw_t r = {0, 0, 4096, 0, 0, 0};
    imu_euler_t e;
    srv_imu_filter_init(50);
    for (unsigned i = 0; i < 99; i++)
        srv_imu_filter_update(&r, 0.02f, &e);
    assert(!srv_imu_filter_is_calibrated());
    srv_imu_filter_update(&r, 0.02f, &e);
    assert(srv_imu_filter_is_calibrated() && fabsf(e.pitch) < 0.01f && fabsf(e.roll) < 0.01f);
    r.gz = 1476;
    for (unsigned i = 0; i < 50; i++)
        srv_imu_filter_update(&r, 0.02f, &e);
    assert(e.yaw > 89 && e.yaw < 91);
    r.ax = r.ay = r.az = r.gz = 0;
    srv_imu_filter_update(&r, 0.02f, &e);
    assert(isfinite(e.pitch) && isfinite(e.roll));
    srv_imu_filter_update(&r, NAN, &e);
    assert(isfinite(e.yaw));
    srv_imu_filter_calibrate_gyro();
    r.az = 4096;
    r.gx = 1000;
    for (unsigned i = 0; i < 110; i++)
        srv_imu_filter_update(&r, 0.02f, &e);
    assert(!srv_imu_filter_is_calibrated());
    r.gx = 0;
    r.ay = 2896;
    r.az = 2896;
    for (unsigned i = 0; i < 101; i++)
        srv_imu_filter_update(&r, 0.02f, &e);
    assert(fabsf(e.roll - 45) < 0.1f);
}
int main(void)
{
    protocols();
    reliable_link();
    inputs();
    imu();
    puts("Step 11: protocol/noise/retry/wrap/key/calibration/filter checks PASS");
    return 0;
}
