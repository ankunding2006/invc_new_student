#include "srv_protocol.h"
#include "system_config.h"
#include <string.h>
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static int16_t signed16(const uint8_t *p)
{
    uint16_t v = get16(p);
    return (int16_t)((v & 0x8000U) ? (int32_t)v - 65536 : (int32_t)v);
}
void srv_protocol_init(void)
{
}
uint8_t srv_protocol_calc_checksum(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0;
    if (p)
        while (n--)
            s = (uint8_t)(s + *p++);
    return s;
}
static void frame_header(uint8_t *o, uint8_t seq, uint8_t len, uint8_t cmd)
{
    o[0] = 0xAA;
    o[1] = 0x55;
    o[2] = seq;
    o[3] = len;
    o[4] = cmd;
}
uint16_t srv_protocol_pack(uint8_t seq, const telemetry_payload_t *p, uint8_t *o, uint16_t cap)
{
    if (!p || !o || cap < 23)
        return 0;
    frame_header(o, seq, 16, PROTOCOL_CMD_TELEMETRY);
    put16(o + 5, (uint16_t)p->joy_x_raw);
    put16(o + 7, (uint16_t)p->joy_y_raw);
    put16(o + 9, p->joy_x_mv);
    put16(o + 11, p->joy_y_mv);
    o[13] = p->key_mask;
    o[14] = p->switch_mask;
    put16(o + 15, (uint16_t)p->pitch_cd);
    put16(o + 17, (uint16_t)p->roll_cd);
    put16(o + 19, (uint16_t)p->yaw_cd);
    o[21] = srv_protocol_calc_checksum(o + 2, 19);
    o[22] = 0x0D;
    return 23;
}
bool srv_protocol_unpack(const protocol_packet_t *p, telemetry_payload_t *o)
{
    if (!p || !o || p->cmd != PROTOCOL_CMD_TELEMETRY || p->len != 16)
        return false;
    o->joy_x_raw = signed16(p->payload);
    o->joy_y_raw = signed16(p->payload + 2);
    o->joy_x_mv = get16(p->payload + 4);
    o->joy_y_mv = get16(p->payload + 6);
    o->key_mask = p->payload[8];
    o->switch_mask = p->payload[9];
    o->pitch_cd = signed16(p->payload + 10);
    o->roll_cd = signed16(p->payload + 12);
    o->yaw_cd = signed16(p->payload + 14);
    return true;
}
/* CRC-16/CCITT-FALSE binds the ACK to all 23 bytes, beyond the 8-bit seq. */
uint16_t srv_protocol_token(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFF;
    if (!p)
        return 0;
    while (n--)
    {
        crc ^= (uint16_t)*p++ << 8;
        for (unsigned b = 0; b < 8; b++)
            crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
    }
    return crc;
}
uint16_t srv_protocol_pack_ack(uint8_t seq, uint16_t token, uint8_t *o, uint16_t cap)
{
    if (!o || cap < 9)
        return 0;
    frame_header(o, seq, 2, PROTOCOL_CMD_ACK);
    put16(o + 5, token);
    o[7] = srv_protocol_calc_checksum(o + 2, 5);
    o[8] = 0x0D;
    return 9;
}
void protocol_parser_init(protocol_parser_t *p)
{
    if (p)
        memset(p, 0, sizeof(*p));
}
static void discard_first(protocol_parser_t *p)
{
    if (p->used)
    {
        --p->used;
        memmove(p->bytes, p->bytes + 1, p->used);
    }
}
bool protocol_parser_feed(protocol_parser_t *p, uint8_t byte, uint32_t now, protocol_packet_t *o)
{
    if (!p || !o)
        return false;
    if (p->used && (uint32_t)(now - p->last_byte_ms) > PARSER_GAP_MS)
    {
        p->used = 0;
        p->errors++;
    }
    p->last_byte_ms = now;
    if (p->used >= sizeof(p->bytes))
    {
        discard_first(p);
        p->errors++;
    }
    p->bytes[p->used++] = byte;
    /* Shift one byte on error: preserves a valid frame embedded in a bad prefix. */
    while (p->used)
    {
        if (p->bytes[0] != 0xAA)
        {
            discard_first(p);
            continue;
        }
        if (p->used < 2)
            return false;
        if (p->bytes[1] != 0x55)
        {
            discard_first(p);
            continue;
        }
        if (p->used < 4)
            return false;
        uint8_t len = p->bytes[3];
        if (len != 16 && len != 2)
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        if (p->used < 5)
            return false;
        if (!((len == 16 && p->bytes[4] == PROTOCOL_CMD_TELEMETRY) ||
              (len == 2 && p->bytes[4] == PROTOCOL_CMD_ACK)))
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        uint8_t total = (uint8_t)(len + 7);
        if (p->used < total)
            return false;
        if (p->bytes[total - 1] != 0x0D ||
            p->bytes[total - 2] != srv_protocol_calc_checksum(p->bytes + 2, (uint16_t)(len + 3)))
        {
            p->errors++;
            discard_first(p);
            continue;
        }
        o->seq = p->bytes[2];
        o->cmd = p->bytes[4];
        o->len = len;
        memcpy(o->payload, p->bytes + 5, len);
        p->used = (uint8_t)(p->used - total);
        memmove(p->bytes, p->bytes + total, p->used);
        return true;
    }
    return false;
}
